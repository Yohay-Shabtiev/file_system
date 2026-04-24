#pragma once

#include <cstdint>
#include "../../includes/fs_constants.hpp"
#include "../../includes/fs_status.hpp"

struct RpcEntry
{
    int inode_id;
    char name[ENTRY_NAME_LENGTH + 1];
};

enum class RpcEntryType : uint32_t
{
    UNKNOWN_ENTRY_TYPE,
    FILE,
    DIRECTORY
};

struct RpcInode
{
    RpcEntryType type;
    uint32_t size;
    uint32_t blocks_used;
    uint32_t link_count;
};

const int MAX_DIR_ENTRIES = BLOCK_SIZE / sizeof(RpcEntry);

enum class RpcOperation : uint32_t
{
    CREATE_FILE,
    GETATTR,
    LOOKUP,
    READ,
    WRITE,
    DELETE,
    MKDIR,
    READDIR
};

enum class RpcStatus : uint32_t
{
    OK,
    SyscallError,
    NotFound,
    NotEmpty,
    FullDisk,
    UnknownError,
    OperationNotFound
};

struct RpcHeader
{
    RpcOperation operation; // the operation
    uint32_t payload_size;  // the sent massage size
};

struct CreateFileRequest
{
    RpcHeader header;
    int parent_inode_id;
    char file_name[ENTRY_NAME_LENGTH + 1]; // + 1 for '\0'
};

struct CreateFileResponse
{
    RpcHeader header;
    RpcStatus status;
    int new_inode_id;
};

struct ReadRequest
{
    RpcHeader header;
    int inode_id;
    uint32_t read_offset;
    uint32_t bytes_to_read;
};

struct ReadResponse
{
    RpcHeader header;
    RpcStatus status;
    uint32_t bytes_read;
    uint8_t data[BLOCK_SIZE];
};

struct WriteRequest
{
    RpcHeader header;
    int inode_id;
    uint32_t write_offset;
    uint32_t data_size;
    uint8_t data[BLOCK_SIZE];
};

struct WriteResponse
{
    RpcHeader header;
    RpcStatus status;
    uint32_t bytes_written;
};

struct DeleteRequest
{
    RpcHeader header;
    int parent_inode_id;
    int inode_id;
};

struct DeleteResponse
{
    RpcHeader header;
    RpcStatus status;
};

struct MkdirRequest
{
    RpcHeader header;
    int parent_inode_id;
    char directory_name[ENTRY_NAME_LENGTH + 1];
};

struct MkdirResponse
{
    RpcHeader header;
    RpcStatus status;
    int new_inode_id;
};

struct ReaddirRequest
{
    RpcHeader header;
    int inode_id;
    uint32_t page;
};

struct ReaddirResponse
{
    RpcHeader header;
    RpcStatus status;
    int entry_count;
    RpcEntry entries[MAX_DIR_ENTRIES];
};

struct GetattrRequest
{
    RpcHeader header;
    int inode_id;
};

struct GetattrResponse
{
    RpcHeader header;
    RpcStatus status;
    RpcInode inode;
};

struct LookupRequest
{
    RpcHeader header;
    int parent_inode_id;
    char entry_name[ENTRY_NAME_LENGTH + 1];
};

struct LookupResponse
{
    RpcHeader header;
    RpcStatus status;
    int inode_id;
};