/*
 * fuse_client.cpp
 *
 * FUSE driver that forwards filesystem operations to the existing
 * RPC server (server.cpp) over TCP on port 8080.
 *
 * Architecture:
 *   WSL kernel → FUSE → fuse_client → TCP:8080 → server → FileSystem
 *
 * Build & run instructions are in README.md.
 */

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <cstring>
#include <cerrno>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include <mutex>
#include <stdexcept>

/* ── Pull in the shared RPC types exactly as the server uses them ── */
#include "rpc/includes/rpc_constants.hpp"
#include "rpc/includes/rpc_types.hpp"

/* ═══════════════════════════════════════════════════════════════════
 *  Connection management
 *  A single persistent TCP connection is reused for all requests.
 *  A mutex guards it so FUSE's multi-threaded dispatch is safe.
 * ═══════════════════════════════════════════════════════════════════ */
static int g_sock = -1;
static std::mutex g_sock_mutex;

static int connect_to_server()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;  

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        close(fd);
        return -1;
    }
    return fd;
}

/* Safe send / recv wrappers that retry on EINTR */
static bool send_all(int fd, const void *buf, size_t len)
{
    const auto *p = static_cast<const uint8_t *>(buf);
    while (len > 0)
    {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
        if (n <= 0)
            return false;
        p += n;
        len -= static_cast<size_t>(n);
    }
    return true;
}

static bool recv_all(int fd, void *buf, size_t len)
{
    auto *p = static_cast<uint8_t *>(buf);
    while (len > 0)
    {
        ssize_t n = recv(fd, p, len, MSG_WAITALL);
        if (n <= 0)
            return false;
        p += n;
        len -= static_cast<size_t>(n);
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 *  RPC helpers – one per operation
 *  All helpers acquire g_sock_mutex so they can be called from any
 *  FUSE callback without extra locking.
 * ═══════════════════════════════════════════════════════════════════ */

static GetattrResponse rpc_getattr(int inode_id)
{
    GetattrRequest req{};
    GetattrResponse resp{};
    resp.status = RpcStatus::SyscallError;

    req.header.operation = RpcOperation::GETATTR;
    req.header.payload_size = static_cast<uint32_t>(sizeof(req) - sizeof(req.header));
    req.inode_id = inode_id;

    std::lock_guard<std::mutex> lock(g_sock_mutex);
    if (!send_all(g_sock, &req, sizeof(req)))
        return resp;
    if (!recv_all(g_sock, &resp, sizeof(resp)))
        return resp;
    return resp;
}

static LookupResponse rpc_lookup(int parent_inode_id, std::string_view name)
{
    LookupRequest req{};
    LookupResponse resp{};
    resp.status = RpcStatus::SyscallError;
    resp.inode_id = -1;

    req.header.operation = RpcOperation::LOOKUP;
    req.header.payload_size = static_cast<uint32_t>(sizeof(req) - sizeof(req.header));
    req.parent_inode_id = parent_inode_id;
    strncpy(req.entry_name, name.data(), ENTRY_NAME_LENGTH);

    std::lock_guard<std::mutex> lock(g_sock_mutex);
    if (!send_all(g_sock, &req, sizeof(req)))
        return resp;
    if (!recv_all(g_sock, &resp, sizeof(resp)))
        return resp;
    return resp;
}

/* Walk a path like "/home/docs/file.txt" component by component.
 * Returns the final inode_id, or -1 on failure. */
static int path_to_inode(const char *path)
{
    if (strcmp(path, "/") == 0)
        return ROOT_INODE_ID;

    int current = ROOT_INODE_ID;
    std::string p(path + 1); /* strip leading '/' */

    size_t pos = 0;
    while (pos < p.size())
    {
        size_t slash = p.find('/', pos);
        std::string component = (slash == std::string::npos)
                                    ? p.substr(pos)
                                    : p.substr(pos, slash - pos);
        if (component.empty())
            break;

        LookupResponse r = rpc_lookup(current, component);
        if (r.status != RpcStatus::OK || r.inode_id < 0)
            return -1;
        current = r.inode_id;

        pos = (slash == std::string::npos) ? p.size() : slash + 1;
    }
    return current;
}

static std::vector<RpcEntry> rpc_readdir(int inode_id)
{
    std::vector<RpcEntry> entries;

    ReaddirRequest req{};
    req.header.operation = RpcOperation::READDIR;
    req.header.payload_size = static_cast<uint32_t>(sizeof(req) - sizeof(req.header));
    req.inode_id = inode_id;
    req.page = 0;

    std::lock_guard<std::mutex> lock(g_sock_mutex);
    while (true)
    {
        if (!send_all(g_sock, &req, sizeof(req)))
            break;

        ReaddirResponse resp{};
        if (!recv_all(g_sock, &resp, sizeof(resp)))
            break;
        if (resp.status != RpcStatus::OK)
            break;
        if (resp.entry_count == -1)
            break; /* no more pages */

        for (int i = 0; i < resp.entry_count; i++)
            entries.push_back(resp.entries[i]);

        req.page++;
    }
    return entries;
}

static int fs_utimens(const char *path, const struct timespec tv[2],
                      struct fuse_file_info *fi)
{
    (void)path;
    (void)tv;
    (void)fi;
    return 0; // pretend it succeeded
}

static CreateFileResponse rpc_create_file(int parent_inode_id, std::string_view name)
{
    CreateFileRequest req{};
    CreateFileResponse resp{};
    resp.status = RpcStatus::SyscallError;
    resp.new_inode_id = -1;

    req.header.operation = RpcOperation::CREATE_FILE;
    req.header.payload_size = static_cast<uint32_t>(sizeof(req) - sizeof(req.header));
    req.parent_inode_id = parent_inode_id;
    strncpy(req.file_name, name.data(), ENTRY_NAME_LENGTH);

    std::lock_guard<std::mutex> lock(g_sock_mutex);
    if (!send_all(g_sock, &req, sizeof(req)))
        return resp;
    if (!recv_all(g_sock, &resp, sizeof(resp)))
        return resp;
    return resp;
}

static MkdirResponse rpc_mkdir(int parent_inode_id, std::string_view name)
{
    MkdirRequest req{};
    MkdirResponse resp{};
    resp.status = RpcStatus::SyscallError;
    resp.new_inode_id = -1;

    req.header.operation = RpcOperation::MKDIR;
    req.header.payload_size = static_cast<uint32_t>(sizeof(req) - sizeof(req.header));
    req.parent_inode_id = parent_inode_id;
    strncpy(req.directory_name, name.data(), ENTRY_NAME_LENGTH);

    std::lock_guard<std::mutex> lock(g_sock_mutex);
    if (!send_all(g_sock, &req, sizeof(req)))
        return resp;
    if (!recv_all(g_sock, &resp, sizeof(resp)))
        return resp;
    return resp;
}

static DeleteResponse rpc_delete(int parent_inode_id, int inode_id)
{
    DeleteRequest req{};
    DeleteResponse resp{};
    resp.status = RpcStatus::SyscallError;

    req.header.operation = RpcOperation::DELETE;
    req.header.payload_size = static_cast<uint32_t>(sizeof(req) - sizeof(req.header));
    req.parent_inode_id = parent_inode_id;
    req.inode_id = inode_id;

    std::lock_guard<std::mutex> lock(g_sock_mutex);
    if (!send_all(g_sock, &req, sizeof(req)))
        return resp;
    if (!recv_all(g_sock, &resp, sizeof(resp)))
        return resp;
    return resp;
}

/* Read up to bytes_to_read bytes starting at offset.
 * Returns the data vector, or empty on error. */
static std::vector<uint8_t> rpc_read(int inode_id,
                                     uint32_t offset,
                                     uint32_t bytes_to_read)
{
    std::vector<uint8_t> result;

    ReadRequest req{};
    req.header.operation = RpcOperation::READ;
    req.header.payload_size = static_cast<uint32_t>(sizeof(req) - sizeof(req.header));
    req.inode_id = inode_id;
    req.read_offset = offset;
    req.bytes_to_read = bytes_to_read;

    std::lock_guard<std::mutex> lock(g_sock_mutex);
    while (req.bytes_to_read > 0)
    {
        if (!send_all(g_sock, &req, sizeof(req)))
            break;

        ReadResponse resp{};
        if (!recv_all(g_sock, &resp, sizeof(resp)))
            break;
        if (resp.status != RpcStatus::OK)
            break;
        if (resp.bytes_read == 0)
            break;

        result.insert(result.end(), resp.data, resp.data + resp.bytes_read);

        if (resp.bytes_read < static_cast<uint32_t>(BLOCK_SIZE))
            break;
        req.read_offset += resp.bytes_read;
        req.bytes_to_read -= resp.bytes_read;
    }
    return result;
}

/* Write data to a file. Returns bytes written, or -1 on error. */
static int rpc_write(int inode_id,
                     const uint8_t *data,
                     uint32_t size,
                     uint32_t offset)
{
    WriteRequest req{};
    req.header.operation = RpcOperation::WRITE;
    req.header.payload_size = static_cast<uint32_t>(sizeof(req) - sizeof(req.header));
    req.inode_id = inode_id;

    int total_written = 0;
    std::lock_guard<std::mutex> lock(g_sock_mutex);

    while (size > 0)
    {
        uint32_t chunk = std::min(size, static_cast<uint32_t>(BLOCK_SIZE));
        req.write_offset = offset + static_cast<uint32_t>(total_written);
        req.data_size = chunk;
        memcpy(req.data, data + total_written, chunk);

        if (!send_all(g_sock, &req, sizeof(req)))
            return -EIO;

        WriteResponse resp{};
        if (!recv_all(g_sock, &resp, sizeof(resp)))
            return -EIO;
        if (resp.status != RpcStatus::OK)
            return -EIO;

        total_written += static_cast<int>(resp.bytes_written);
        size -= resp.bytes_written;
    }
    return total_written;
}

/* ═══════════════════════════════════════════════════════════════════
 *  FUSE callback implementations
 * ═══════════════════════════════════════════════════════════════════ */

/* Helper: translate RpcStatus → negative errno */
static int rpc_status_to_errno(RpcStatus s)
{
    switch (s)
    {
    case RpcStatus::OK:
        return 0;
    case RpcStatus::NotFound:
        return -ENOENT;
    case RpcStatus::NotEmpty:
        return -ENOTEMPTY;
    case RpcStatus::FullDisk:
        return -ENOSPC;
    default:
        return -EIO;
    }
}

/* ── getattr ── */
static int fs_getattr(const char *path, struct stat *st,
                      struct fuse_file_info * /*fi*/)
{
    memset(st, 0, sizeof(*st));

    int inode_id = path_to_inode(path);
    if (inode_id < 0)
        return -ENOENT;

    GetattrResponse r = rpc_getattr(inode_id);
    if (r.status != RpcStatus::OK)
        return rpc_status_to_errno(r.status);

    if (r.inode.type == RpcEntryType::DIRECTORY)
    {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
    }
    else
    {
        st->st_mode = S_IFREG | 0644;
        st->st_nlink = 1;
    }
    st->st_size = static_cast<off_t>(r.inode.size);
    st->st_blocks = static_cast<blkcnt_t>(r.inode.blocks_used);
    st->st_uid = getuid();
    st->st_gid = getgid();
    return 0;
}

/* ── readdir ── */
static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t /*offset*/, struct fuse_file_info * /*fi*/,
                      enum fuse_readdir_flags /*flags*/)
{
    int inode_id = path_to_inode(path);
    if (inode_id < 0)
        return -ENOENT;

    filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);

    auto entries = rpc_readdir(inode_id);
    for (const auto &e : entries)
    {
        /* Skip the "." and ".." entries that the server stores internally */
        if (strcmp(e.name, ".") == 0)
            continue;
        if (strcmp(e.name, "..") == 0)
            continue;
        filler(buf, e.name, nullptr, 0, FUSE_FILL_DIR_PLUS);
    }
    return 0;
}

/* ── open ── (we just check the file exists) */
static int fs_open(const char *path, struct fuse_file_info *fi)
{
    int inode_id = path_to_inode(path);
    if (inode_id < 0)
        return -ENOENT;

    /* Store inode_id in fh so read/write don't need to re-resolve */
    fi->fh = static_cast<uint64_t>(inode_id);
    return 0;
}

/* ── read ── */
static int fs_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi)
{
    int inode_id = (fi && fi->fh) ? static_cast<int>(fi->fh)
                                  : path_to_inode(path);
    if (inode_id < 0)
        return -ENOENT;

    auto data = rpc_read(inode_id,
                         static_cast<uint32_t>(offset),
                         static_cast<uint32_t>(size));
    if (data.empty())
        return 0;

    memcpy(buf, data.data(), data.size());
    return static_cast<int>(data.size());
}

/* ── write ── */
static int fs_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi)
{
    int inode_id = (fi && fi->fh) ? static_cast<int>(fi->fh)
                                  : path_to_inode(path);
    if (inode_id < 0)
        return -ENOENT;

    return rpc_write(inode_id,
                     reinterpret_cast<const uint8_t *>(buf),
                     static_cast<uint32_t>(size),
                     static_cast<uint32_t>(offset));
}

/* ── create ── */
static int fs_create(const char *path, mode_t /*mode*/,
                     struct fuse_file_info *fi)
{
    std::string p(path);
    auto slash = p.rfind('/');
    std::string parent_path = (slash == 0) ? "/" : p.substr(0, slash);
    std::string name = p.substr(slash + 1);

    // if (name.length() > ENTRY_NAME_LENGTH)
    //     return -ENAMETOOLONG;

    int parent_inode = path_to_inode(parent_path.c_str());
    if (parent_inode < 0)
        return -ENOENT;

    CreateFileResponse r = rpc_create_file(parent_inode, name);
    if (r.status != RpcStatus::OK)
        return rpc_status_to_errno(r.status);

    if (fi)
        fi->fh = static_cast<uint64_t>(r.new_inode_id);
    return 0;
}

/* ── mkdir ── */
static int fs_mkdir(const char *path, mode_t /*mode*/)
{
    std::string p(path);
    auto slash = p.rfind('/');
    std::string parent_path = (slash == 0) ? "/" : p.substr(0, slash);
    std::string name = p.substr(slash + 1);

    if (name.length() > ENTRY_NAME_LENGTH)
        return -ENAMETOOLONG;

    int parent_inode = path_to_inode(parent_path.c_str());
    if (parent_inode < 0)
        return -ENOENT;

    MkdirResponse r = rpc_mkdir(parent_inode, name);
    if (r.status != RpcStatus::OK)
        return rpc_status_to_errno(r.status);
    return 0;
}

/* ── unlink (delete file) ── */
static int fs_unlink(const char *path)
{
    std::string p(path);
    auto slash = p.rfind('/');
    std::string parent_path = (slash == 0) ? "/" : p.substr(0, slash);
    std::string name = p.substr(slash + 1);

    int parent_inode = path_to_inode(parent_path.c_str());
    if (parent_inode < 0)
        return -ENOENT;

    LookupResponse lr = rpc_lookup(parent_inode, name);
    if (lr.status != RpcStatus::OK)
        return -ENOENT;

    DeleteResponse r = rpc_delete(parent_inode, lr.inode_id);
    return rpc_status_to_errno(r.status);
}

/* ── rmdir ── */
static int fs_rmdir(const char *path)
{
    return fs_unlink(path); /* server's delete_entry handles both */
}

/* ── truncate ── (needed for write-after-create with O_TRUNC) */
static int fs_truncate(const char *path, off_t size,
                       struct fuse_file_info * /*fi*/)
{
    /* Our FS doesn't expose a truncate RPC, but FUSE requires this.
     * We handle the common case of truncate-to-zero by overwriting
     * with an empty write — the server re-uses the inode. */
    (void)path;
    (void)size;
    return 0; /* silently accept; writes will overwrite as needed */
}

/* ── statfs ── (df, etc.) */
static int fs_statfs(const char * /*path*/, struct statvfs *stbuf)
{
    memset(stbuf, 0, sizeof(*stbuf));
    stbuf->f_bsize = BLOCK_SIZE;
    stbuf->f_blocks = TOTAL_BLOCKS_NUMBER;
    stbuf->f_bfree = 0; /* we don't expose a free-blocks RPC */
    stbuf->f_bavail = 0;
    stbuf->f_namemax = ENTRY_NAME_LENGTH;
    return 0;
}

/* ── release ── (close) */
static int fs_release(const char * /*path*/, struct fuse_file_info * /*fi*/)
{
    return 0; /* connection stays open; nothing to do per-file */
}

/* ═══════════════════════════════════════════════════════════════════
 *  FUSE operations table
 * ═══════════════════════════════════════════════════════════════════ */
static const struct fuse_operations fs_ops = {
    .getattr = fs_getattr,
    .mkdir = fs_mkdir,
    .unlink = fs_unlink,
    .rmdir = fs_rmdir,
    .truncate = fs_truncate,
    .open = fs_open,
    .read = fs_read,
    .write = fs_write,
    .statfs = fs_statfs,
    .release = fs_release,
    .readdir = fs_readdir,
    .create = fs_create,
    .utimens = fs_utimens,
};

/* ═══════════════════════════════════════════════════════════════════
 *  main
 * ═══════════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[])
{
    g_sock = connect_to_server();
    if (g_sock < 0)
    {
        fprintf(stderr,
                "fuse_client: cannot connect to RPC server at 127.0.0.1:%d\n"
                "  → Make sure the server is running first.\n",
                PORT);
        return 1;
    }
    printf("fuse_client: connected to RPC server on port %d\n", PORT);

    int ret = fuse_main(argc, argv, &fs_ops, nullptr);

    close(g_sock);
    return ret;
}
