// server.cpp
#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>

#include <sys/socket.h> // socket, bind, listen, accept
#include <netinet/in.h> // sockaddr_in
#include <unistd.h>     // close

#include "includes/rpc_constants.hpp"
#include "includes/rpc_types.hpp"
#include "../includes/file_system.hpp"
#include "../includes/in_memory_block_device.hpp"

RpcStatus handle_client(int client_fd, FileSystem &fs, std::mutex &fs_mutex);
CreateFileResponse handle_create_file(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size);
MkdirResponse handle_mkdir(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size);
RpcStatus fs_status_to_rpc_status(FileSystemStatus status);
RpcEntryType fs_entry_type_to_rpc_status(EntryType type);
ReadResponse handle_read(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size);
WriteResponse handle_write(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size);
DeleteResponse handle_delete(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size);
ReaddirResponse handle_read_dir(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size);
GetattrResponse handle_getattr(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size);
LookupResponse handle_lookup(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size);

int main()
{
    std::mutex fs_mutex;
    // step 1 — create the socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        std::cerr << "socket() in server.main() failed" << std::endl;
        return 1;
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // step 2 — bind to a port
    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;         // IPv4
    server_address.sin_addr.s_addr = INADDR_ANY; // accept from any IP
    server_address.sin_port = htons(PORT);       // port 8080

    if (bind(server_fd, (sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        std::cerr << "bind() in server.main() failed" << std::endl;
        return 1;
    }

    // step 3 — listen
    if (listen(server_fd, 5) < 0) // 5 = max pending connections
    {
        std::cerr << "listen() in server.main() failed" << std::endl;
        return 1;
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    // recieving data
    InMemoryBlockDevice device(TOTAL_BLOCKS_NUMBER * BLOCK_SIZE);
    FileSystem fs(device);
    fs.format();

    // step 4 — accept a client
    while (true)
    {

        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);
        int client_fd = accept(server_fd, (sockaddr *)&client_address, &client_len);
        if (client_fd < 0)
        {
            std::cerr << "accept() in server.main() failed" << std::endl;
            continue;
        }

        std::cout << "Client connected!" << std::endl;
        std::thread t(handle_client, client_fd, std::ref(fs), std::ref(fs_mutex));
        t.detach();
    }

    // while (handle_client(client_fd, fs) == RpcStatus::OK)
    //     ;

    close(server_fd);
    return 0;
}

RpcEntryType fs_entry_type_to_rpc_status(EntryType type)
{
    if (type == EntryType::File)
        return RpcEntryType::FILE;

    if (type == EntryType::Directory)
        return RpcEntryType::DIRECTORY;

    return RpcEntryType::UNKNOWN_ENTRY_TYPE;
}

RpcStatus fs_status_to_rpc_status(FileSystemStatus status)
{
    switch (status)
    {
    case FileSystemStatus::OK:
        return RpcStatus::OK;
    case FileSystemStatus::NotFound:
        return RpcStatus::NotFound;
    case FileSystemStatus::FullDisk:
        return RpcStatus::FullDisk;
    case FileSystemStatus::NotEmpty:
        return RpcStatus::NotEmpty;
    default:
        return RpcStatus::UnknownError;
    }
}

RpcStatus handle_client(int client_fd, FileSystem &fs, std::mutex &fs_mutex)
{
    while (true)
    {
        // reciving the header
        RpcHeader request_header;

        int bytes = recv(client_fd, &request_header, sizeof(request_header), 0);
        if (bytes < 0)
        {
            std::cerr << "recv() server.handle_client() failed" << std::endl;
            return RpcStatus::SyscallError;
        }

        if (bytes == 0)
        {
            close(client_fd);
            return RpcStatus::OK;
        }

        // executing the request
        switch (request_header.operation)
        {
        case RpcOperation::CREATE_FILE:
        {
            CreateFileResponse response = handle_create_file(client_fd, fs, fs_mutex, request_header.payload_size);
            send(client_fd, &response, sizeof(response), 0);
            break;
        }

        case RpcOperation::MKDIR:
        {
            MkdirResponse response = handle_mkdir(client_fd, fs, fs_mutex, request_header.payload_size);
            send(client_fd, &response, sizeof(response), 0);
            break;
        }

        case RpcOperation::READ:
        {
            ReadResponse response = handle_read(client_fd, fs, fs_mutex, request_header.payload_size);
            send(client_fd, &response, sizeof(response), 0);
            break;
        }

        case RpcOperation::WRITE:
        {
            WriteResponse response = handle_write(client_fd, fs, fs_mutex, request_header.payload_size);
            send(client_fd, &response, sizeof(response), 0);
            break;
        }

        case RpcOperation::DELETE:
        {
            DeleteResponse response = handle_delete(client_fd, fs, fs_mutex, request_header.payload_size);
            send(client_fd, &response, sizeof(response), 0);
            break;
        }

        case RpcOperation::READDIR:
        {
            ReaddirResponse response = handle_read_dir(client_fd, fs, fs_mutex, request_header.payload_size);
            send(client_fd, &response, sizeof(response), 0);
            break;
        }

        case RpcOperation::GETATTR:
        {
            GetattrResponse response = handle_getattr(client_fd, fs, fs_mutex, request_header.payload_size);
            send(client_fd, &response, sizeof(response), 0);
            break;
        }

        case RpcOperation::LOOKUP:
        {
            LookupResponse response = handle_lookup(client_fd, fs, fs_mutex, request_header.payload_size);
            send(client_fd, &response, sizeof(response), 0);
            break;
        }

        default:
            std::cerr << "unknown operation received" << std::endl;
            break;
        }
    }

    return RpcStatus::OK;
}

CreateFileResponse handle_create_file(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size)
{
    CreateFileRequest request;
    CreateFileResponse response;

    if (recv(client_fd, &request.parent_inode_id, payload_size, 0) < 0)
    {
        std::cerr << "recv() in server.handle_create_file() failed" << std::endl;
        response.status = RpcStatus::SyscallError;
        response.new_inode_id = -1;
        return response;
    }

    {
        std::lock_guard<std::mutex> lock(fs_mutex);
        auto create_file_res = fs.create_file(request.parent_inode_id, request.file_name);
        if (!create_file_res.has_value())
        {
            response.status = fs_status_to_rpc_status(create_file_res.error());
            response.new_inode_id = -1;
            return response;
        }

        response.new_inode_id = create_file_res.value();

        auto attributes_res = fs.get_attributes(response.new_inode_id);
        if (!attributes_res.has_value())
            return response;

        InodeAttributes attributes = attributes_res.value();

        // std::cout << "Test: the inode size is: " << attributes.size
        //           << " the entry type is: " << int(attributes.type)
        //           << std::endl;
    }
    response.status = RpcStatus::OK;

    return response;
}

MkdirResponse handle_mkdir(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size)
{
    MkdirRequest request;
    MkdirResponse response;

    if (recv(client_fd, &request.parent_inode_id, payload_size, 0) < 0)
    {
        std::cerr << "recv() in server.handle_mkdir() failed" << std::endl;
        response.new_inode_id = -1;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    {
        std::lock_guard<std::mutex> lock(fs_mutex);
        auto new_dir_res = fs.create_directory(request.parent_inode_id, request.directory_name);
        if (!new_dir_res.has_value())
        {
            response.new_inode_id = -1;
            response.status = fs_status_to_rpc_status(new_dir_res.error());
            return response;
        }

        response.new_inode_id = new_dir_res.value();
    }
    response.status = RpcStatus::OK;

    return response;
}

ReadResponse handle_read(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size)
{
    ReadResponse response;
    ReadRequest request;

    // reciving the request details
    if (recv(client_fd, &request.inode_id, payload_size, 0) < 0)
    {
        std::cerr << "recv() in server.handle_mkdir() failed" << std::endl;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    uint8_t buffer[BLOCK_SIZE];
    std::span data_span(buffer, BLOCK_SIZE); // wrap buffer as a span

    {
        std::lock_guard<std::mutex> lock(fs_mutex);
        auto read_res = fs.read_file(request.inode_id, data_span, request.read_offset);
        // fs.read_file() failed
        if (!read_res.has_value())
        {
            std::cerr << "fs.read_file() in server.handle_read() failed" << std::endl;
            response.status = fs_status_to_rpc_status(read_res.error());
            return response;
        }

        response.bytes_read = read_res.value();
    }
    memcpy(response.data, buffer, response.bytes_read);
    response.status = RpcStatus::OK;

    return response;
}

WriteResponse handle_write(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size)
{
    WriteRequest request;
    WriteResponse response;

    // reciving the request
    if (recv(client_fd, &request.inode_id, payload_size, 0) < 0)
    {
        std::cerr << "recv() in server.handle_write() failed" << std::endl;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    std::span<const uint8_t> data_span(request.data, request.data_size);

    {
        std::lock_guard<std::mutex> lock(fs_mutex);
        auto write_res = fs.write_file(request.inode_id, data_span, request.write_offset);
        if (!write_res.has_value())
        {
            std::cerr << "fs.write_file() in server.handle_write() failed" << std::endl;
            response.status = fs_status_to_rpc_status(write_res.error());
            return response;
        }

        response.bytes_written = write_res.value();
    }
    response.status = RpcStatus::OK;

    return response;
}

DeleteResponse handle_delete(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size)
{
    DeleteRequest request;
    DeleteResponse response;

    if (recv(client_fd, &request.parent_inode_id, payload_size, 0) < 0)
    {
        std::cerr << "recv() in server.handle_delete() failed" << std::endl;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    {
        std::lock_guard<std::mutex> lock(fs_mutex);
        auto status = fs.delete_entry(request.parent_inode_id, request.inode_id);
        response.status = fs_status_to_rpc_status(status);
    }

    return response;
}

ReaddirResponse handle_read_dir(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size)
{
    ReaddirResponse response;
    ReaddirRequest request;
    std::vector<Entry> entries;

    if (recv(client_fd, &request.inode_id, payload_size, 0) < 0)
    {
        std::cerr << "recv() in server.handle_read_dir() failed" << std::endl;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    if (request.page >= TOTAL_DIRECT_BLOCKS)
    {
        response.entry_count = -1;
        response.status = RpcStatus::OK;
        return response;
    }

    {
        std::lock_guard<std::mutex> lock(fs_mutex);
        auto entries_res = fs.list_directory_content(request.inode_id, request.page);

        if (!entries_res.has_value())
        {
            response.entry_count = -1;
            response.status = RpcStatus::OK;
            return response;
        }

        entries = entries_res.value();
    }

    response.entry_count = 0;

    // copying the data
    for (int i = 0; i < (int)entries.size(); i++)
    {
        if (entries[i].inode_id == -1)
            continue;

        RpcEntry rpc_entry{};
        rpc_entry.inode_id = entries[i].inode_id;
        memcpy(rpc_entry.name, entries[i].name, strlen(entries[i].name) + 1);

        response.entries[response.entry_count++] = rpc_entry;
    }

    response.status = RpcStatus::OK;

    return response;
}

GetattrResponse handle_getattr(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size)
{
    GetattrRequest request;
    GetattrResponse response;

    if (recv(client_fd, &request.inode_id, payload_size, 0) < 0)
    {
        std::cerr << "recv() in server.handle_getattr() failed" << std::endl;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    // std::cout << "Test: the inode_id is: " << request.inode_id << std::endl;

    InodeAttributes inode_attr;
    {
        std::lock_guard<std::mutex> lock(fs_mutex);

        auto inode_res = fs.get_attributes(request.inode_id);
        if (!inode_res.has_value())
        {
            response.status = fs_status_to_rpc_status(inode_res.error());
            return response;
        }

        inode_attr = inode_res.value();
    }

    response.inode.type = fs_entry_type_to_rpc_status(inode_attr.type);
    response.inode.size = inode_attr.size;
    response.inode.link_count = inode_attr.link_count;
    response.inode.blocks_used = inode_attr.blocks_used;
    response.inode.size = inode_attr.size;

    response.status = RpcStatus::OK;

    return response;
}

LookupResponse handle_lookup(int client_fd, FileSystem &fs, std::mutex &fs_mutex, uint32_t payload_size)
{
    LookupRequest request;
    LookupResponse response;

    if (recv(client_fd, &request.parent_inode_id, payload_size, 0) < 0)
    {
        std::cerr << "recv() in server.handle_lookup() failed" << std::endl;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    // std::cout << "Test: the inode_id is: " << request.parent_inode_id << std::endl;
    // std::cout << "Test: the inode_id is: " << request.entry_name << std::endl;

    {
        std::lock_guard<std::mutex> lock(fs_mutex);

        auto entry_res = fs.lookup(request.parent_inode_id, request.entry_name);
        if (!entry_res.has_value())
        {
            response.status = fs_status_to_rpc_status(entry_res.error());
            return response;
        }

        response.inode_id = entry_res.value().inode_id;
    }

    std::cout << "Test: the inode_id is: " << response.inode_id << std::endl;

    response.status = RpcStatus::OK;

    return response;
}
