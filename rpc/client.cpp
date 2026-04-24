#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <string_view>
#include <expected>
#include <vector>

#include "includes/rpc_constants.hpp"
#include "includes/rpc_types.hpp"

CreateFileResponse create_file(int client_fd, int parent_inode_id, std::string_view file_name);
MkdirResponse create_dir(int client_fd, int parent_inode_id, std::string_view dir_name);
std::expected<std::vector<uint8_t>, RpcStatus> read_file(int client_fd, int inode_id, uint32_t read_offset, uint32_t bytes_to_read);
WriteResponse write_file(int client_fd, int inode_id, const std::vector<uint8_t> &data, uint32_t write_offset);
DeleteResponse delete_entry(int client_fd, int parent_inode_id, int inode_id);
std::expected<std::vector<RpcEntry>, RpcStatus> read_dir(int client_fd, int inode_id);
GetattrResponse getattr(int client_fd, int inode_id);
LookupResponse lookup(int client_fd, int parent_inode_id, std::string_view entry_name);

int main()
{
    // create the socket ( the telephone )
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0)
    {
        std::cerr << "socket() failed" << std::endl;
        return 1;
    }

    // create the contact information
    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);
    server_address.sin_port = htons(PORT);

    // connect to the server
    if (connect(client_fd, (sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        std::cerr << "connect() failed" << std::endl;
        return 1;
    }

    // create a directory structure
    MkdirResponse dir_res = create_dir(client_fd, ROOT_INODE_ID, "home");
    std::cout << "mkdir home: inode=" << dir_res.new_inode_id << std::endl;

    MkdirResponse dir_res2 = create_dir(client_fd, dir_res.new_inode_id, "docs");
    std::cout << "mkdir docs: inode=" << dir_res2.new_inode_id << std::endl;

    CreateFileResponse file_res = create_file(client_fd, dir_res2.new_inode_id, "file.txt");
    std::cout << "create file.txt: inode=" << file_res.new_inode_id << std::endl;

    // now resolve /home/docs/file.txt NFS style - one component at a time
    LookupResponse home = lookup(client_fd, ROOT_INODE_ID, "home");
    std::cout << "lookup home: inode=" << home.inode_id << std::endl;

    LookupResponse docs = lookup(client_fd, home.inode_id, "docs");
    std::cout << "lookup docs: inode=" << docs.inode_id << std::endl;

    LookupResponse file = lookup(client_fd, docs.inode_id, "file.txt");
    std::cout << "lookup file.txt: inode=" << file.inode_id << std::endl;

    close(client_fd);
    return 0;
}

CreateFileResponse create_file(int client_fd, int parent_inode_id, std::string_view file_name)
{
    CreateFileRequest request;
    CreateFileResponse response;

    request.header.operation = RpcOperation::CREATE_FILE;
    request.header.payload_size = sizeof(request) - sizeof(request.header);
    strncpy(request.file_name, file_name.data(), ENTRY_NAME_LENGTH);
    request.parent_inode_id = parent_inode_id;

    // sending the request to the server
    if (send(client_fd, &request, sizeof(request), 0) < 0)
    {
        std::cerr << "send() failed" << std::endl;
        response.new_inode_id = -1;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    // reciving the server response
    if (recv(client_fd, &response, sizeof(response), 0) < 0)
    {
        std::cerr << "recv() failed" << std::endl;
        response.new_inode_id = -1;
        response.status = RpcStatus::SyscallError;
    }

    return response;
}

MkdirResponse create_dir(int client_fd, int parent_inode_id, std::string_view dir_name)
{
    MkdirRequest request;
    MkdirResponse response;

    request.header.operation = RpcOperation::MKDIR;
    request.header.payload_size = sizeof(request) - sizeof(request.header);
    request.parent_inode_id = parent_inode_id;
    strncpy(request.directory_name, dir_name.data(), ENTRY_NAME_LENGTH);

    if (send(client_fd, &request, sizeof(request), 0) < 0)
    {
        std::cerr << "send() client.create_dir() failed" << std::endl;
        response.new_inode_id = -1;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    if (recv(client_fd, &response, sizeof(response), 0) < 0)
    {
        std::cerr << "recv() client.create_dir() failed" << std::endl;
        response.new_inode_id = -1;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    return response;
}

std::expected<std::vector<uint8_t>, RpcStatus> read_file(int client_fd, int inode_id, uint32_t read_offset, uint32_t bytes_to_read)
{
    ReadRequest request;
    ReadResponse response;
    std::vector<uint8_t> v_data;

    // init request
    request.header.operation = RpcOperation::READ;
    request.header.payload_size = sizeof(request) - sizeof(request.header);
    request.inode_id = inode_id;
    request.read_offset = read_offset;
    request.bytes_to_read = bytes_to_read;

    while (request.bytes_to_read > 0)
    {
        // sending the request
        if (send(client_fd, &request, sizeof(request), 0) < 0)
        {
            std::cerr << "send() client.read_file() failed" << std::endl;
            return std::unexpected(RpcStatus::SyscallError);
        }

        // reciving the response
        if (recv(client_fd, &response, sizeof(response), 0) < 0)
        {
            std::cerr << "recv() client.read_file() failed" << std::endl;
            return std::unexpected(RpcStatus::SyscallError);
        }

        if (response.status != RpcStatus::OK)
            return std::unexpected(response.status);
        // copying the current data
        v_data.insert(v_data.end(), response.data, response.data + response.bytes_read);

        if (response.bytes_read < BLOCK_SIZE)
            break;
        // modify request
        request.read_offset += response.bytes_read;
        request.bytes_to_read -= response.bytes_read;
    }

    return v_data;
}

WriteResponse write_file(int client_fd, int inode_id, const std::vector<uint8_t> &data, uint32_t write_offset)
{
    WriteRequest request;
    WriteResponse response;

    request.header.operation = RpcOperation::WRITE;
    request.header.payload_size = sizeof(request) - sizeof(request.header);
    request.inode_id = inode_id;

    uint32_t chunck_size;
    uint32_t total_bytes_written = 0;
    while (true)
    {
        // set the offset
        request.write_offset = write_offset + total_bytes_written;

        chunck_size = std::min(static_cast<uint32_t>(data.size()) - total_bytes_written, static_cast<uint32_t>(BLOCK_SIZE));
        request.data_size = chunck_size;

        memcpy(request.data, data.data() + total_bytes_written, request.data_size);

        if (send(client_fd, &request, sizeof(request), 0) < 0)
        {
            std::cerr << "send() failed client.write_file()" << std::endl;
            response.status = RpcStatus::SyscallError;
            return response;
        }

        if (recv(client_fd, &response, sizeof(response), 0) < 0)
        {
            std::cerr << "recv() failed client.write_file()" << std::endl;
            response.status = RpcStatus::SyscallError;
            return response;
        }

        total_bytes_written += response.bytes_written;

        if (response.status != RpcStatus::OK)
            return response;

        if (chunck_size < BLOCK_SIZE)
            break;
    }

    return response;
}

DeleteResponse delete_entry(int client_fd, int parent_inode_id, int inode_id)
{
    DeleteRequest request;
    DeleteResponse response;

    request.header.operation = RpcOperation::DELETE;
    request.header.payload_size = sizeof(request) - sizeof(request.header);
    request.parent_inode_id = parent_inode_id;
    request.inode_id = inode_id;

    if (send(client_fd, &request, sizeof(request), 0) < 0)
    {
        std::cerr << "send() failed client.delete_entry()" << std::endl;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    if (recv(client_fd, &response, sizeof(response), 0) < 0)
    {
        std::cerr << "recv() failed client.delete_entry()" << std::endl;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    return response;
}

std::expected<std::vector<RpcEntry>, RpcStatus> read_dir(int client_fd, int inode_id)
{
    ReaddirRequest request;
    ReaddirResponse response;
    std::vector<RpcEntry> entries;

    request.header.operation = RpcOperation::READDIR;
    request.header.payload_size = sizeof(request) - sizeof(request.header);
    request.inode_id = inode_id;
    request.page = 0;

    while (true)
    {
        if (send(client_fd, &request, sizeof(request), 0) < 0)
        {
            std::cerr << "send() failed client.read_dir()" << std::endl;
            return std::unexpected(RpcStatus::SyscallError);
        }

        if (recv(client_fd, &response, sizeof(response), 0) < 0)
        {
            std::cerr << "recv() failed client.read_dir()" << std::endl;
            return std::unexpected(RpcStatus::SyscallError);
        }

        if (response.status != RpcStatus::OK)
            return std::unexpected(response.status);

        if (response.entry_count == -1)
            break;

        // copy the entries to the vector
        entries.insert(entries.end(), response.entries, response.entries + response.entry_count);
        request.page += 1;
    }

    return entries;
}

GetattrResponse getattr(int client_fd, int inode_id)
{
    GetattrRequest request;
    GetattrResponse response;

    request.header.operation = RpcOperation::GETATTR;
    request.header.payload_size = sizeof(request) - sizeof(request.header);
    request.inode_id = inode_id;

    if (send(client_fd, &request, sizeof(request), 0) < 0)
    {
        std::cerr << "send() failed client.handle_getattr()" << std::endl;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    if (recv(client_fd, &response, sizeof(response), 0) < 0)
    {
        std::cerr << "recv() failed client.handle_getattr()" << std::endl;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    return response;
}

LookupResponse lookup(int client_fd, int parent_inode_id, std::string_view entry_name)
{
    LookupRequest request;
    LookupResponse response;

    request.header.operation = RpcOperation::LOOKUP;
    request.header.payload_size = sizeof(request) - sizeof(request.header);
    request.parent_inode_id = parent_inode_id;
    strncpy(request.entry_name, entry_name.data(), ENTRY_NAME_LENGTH);

    if (send(client_fd, &request, sizeof(request), 0) < 0)
    {
        std::cerr << "send() failed client.lookup()" << std::endl;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    if (recv(client_fd, &response, sizeof(response), 0) < 0)
    {
        std::cerr << "recv() failed client.lookup()" << std::endl;
        response.status = RpcStatus::SyscallError;
        return response;
    }

    return response;
}