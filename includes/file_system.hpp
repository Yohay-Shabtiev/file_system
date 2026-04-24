#pragma once

#include "fs_constants.hpp"
#include "block_device.hpp"
#include "fs_status.hpp"
#include <string>
#include <vector>
#include <expected>
#include <optional>
#include <string_view>
#include <algorithm>
#include <array>
#include <span>

enum class EntryType
{
    Uninitialized,
    File,
    Directory
};

struct Superblock
{
    int magic;
    int version;
    int total_blocks;
    int root_dir_block_index;
};

struct Inode
{
    EntryType type;
    int size;
    int direct_blocks[TOTAL_DIRECT_BLOCKS];
    int link_count;
};

static_assert(sizeof(Inode) == (3 + 12) * sizeof(int), "Inode must be 60 bytes");

struct InodeAttributes // requires for the RPC GETATTR operation
{
    EntryType type;
    uint32_t size;
    uint32_t blocks_used;
    uint32_t link_count;
};

struct Entry
{
    int inode_id;
    EntryType type;
    char name[ENTRY_NAME_LENGTH + 1];
};

static_assert(sizeof(Entry) == 64 + 8, "padding must be different");

// Global Constants for Layout
const int INODE_SIZE = sizeof(Inode);
const int DIR_ENTRY_SIZE = sizeof(Entry);
const int ENTRIES_PER_BLOCK = BLOCK_SIZE / sizeof(Entry);
const int INODES_PER_BLOCK = BLOCK_SIZE / INODE_SIZE;
const int INODE_TABLE_SIZE = (TOTAL_INODE_NUMBER + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK;

const int ROOT_DIR_BLOCK_INDEX = INODE_TABLE_START_INDEX + INODE_TABLE_SIZE;
const int DATA_START_BLOCK = ROOT_DIR_BLOCK_INDEX + 1;
const int DATA_TABLE_SIZE = TOTAL_BLOCKS_NUMBER - DATA_START_BLOCK;

class FileSystem
{
public:
    explicit FileSystem(BlockDevice &_device);

    void format();

    /********** Public API ************/

    std::expected<Entry, FileSystemStatus> lookup(int dir_inode_id, std::string_view entry_name);
    FileSystemStatus delete_entry(int parent_inode_id, int inode_id);
    std::expected<InodeAttributes, FileSystemStatus> get_attributes(int inode_id);

    // File operations
    std::expected<int, FileSystemStatus> create_file(int parent_inode_id, std::string_view file_name);
    std::expected<size_t, FileSystemStatus> write_file(int inode_id, std::span<const uint8_t> data, size_t offset);
    std::expected<size_t, FileSystemStatus> read_file(int inode_id, std::span<uint8_t> data, size_t offset);

    // Directory operations
    std::expected<int, FileSystemStatus> create_directory(int parent_inode_id, std::string_view dir_name);
    std::expected<std::vector<Entry>, FileSystemStatus> list_directory_content(int inode_id, uint32_t entry_offset);

    // Testing helpers
    const std::array<int, INODE_TABLE_SIZE> &get_inode_blocks_table() const { return inode_table_blocks; }

    friend class DataManagerTest;
    friend class InodeManagerTest;

private:
    BlockDevice &device;
    std::array<int, INODE_TABLE_SIZE> inode_table_blocks;
    bool is_formatted;
    Superblock superblock;

    /********** Templates ************/

    template <typename T, typename Predicate>
    std::optional<T> get_element(const int *block_indices, int num_indices, int table_base_offset, int &out_absolute_block, Predicate is_match)
    {
        uint8_t buffer[BLOCK_SIZE];
        for (int i = 0; i < num_indices; i++)
        {
            out_absolute_block = block_indices[i] + table_base_offset;
            if (block_indices[i] == -1)
                continue;

            auto *start = get_block_ptr<T>(out_absolute_block, buffer);
            auto *end = start + BLOCK_SIZE / sizeof(T);
            auto it = std::find_if(start, end, is_match);
            if (it != end)
                return *it;
        }
        out_absolute_block = -1;
        return std::nullopt;
    }

    template <typename T>
    T *get_block_ptr(int absolute_block_number, uint8_t *buffer)
    {
        device.read_block(absolute_block_number, buffer);
        return reinterpret_cast<T *>(buffer);
    }

    template <typename T>
    void initialize_block(uint8_t *buffer)
    {
        T *elements = reinterpret_cast<T *>(buffer);
        T empty_element = {};
        set_as_empty(empty_element);
        std::fill(elements, elements + BLOCK_SIZE / sizeof(T), empty_element);
    }

    /********** Initialization ************/
    void format_superblock();
    FileSystemStatus init_root_directory();
    FileSystemStatus init_directory_entries(int inode_id, int parent_inode_id);
    FileSystemStatus init_inode_bitmap_on_format();
    FileSystemStatus init_inode_table_on_format() const;
    FileSystemStatus init_data_bitmap_on_format();
    void init_data_blocks_on_format();

    /********** Inode Management ************/
    Inode create_inode(EntryType type);
    std::expected<Inode, FileSystemStatus> get_inode(int inode_id);
    std::expected<int, FileSystemStatus> allocate_inode();
    std::expected<int, FileSystemStatus> create_new_inode(EntryType type, int parent_inode_id, std::string_view name);
    FileSystemStatus write_inode(int inode_id, const Inode &inode);
    FileSystemStatus free_inode(int inode_id);

    /********** Data Block Management ************/
    std::expected<int, FileSystemStatus> allocate_data_block();
    std::expected<int, FileSystemStatus> get_block_index(Inode &inode, int target_block);
    std::expected<int, FileSystemStatus> get_or_allocate_block_index(int inode_id, Inode &inode, int target_block);
    FileSystemStatus free_data_block(int data_block_number);

    /********** Directory & Entry Management ************/
    Entry create_entry(EntryType type, int inode_id, std::string_view name);
    void set_as_empty(Entry &entry);
    void set_as_empty(Inode &inode);
    FileSystemStatus add_entry(int parent_inode_id, Entry &entry);
    FileSystemStatus add_entry_to_parent(int parent_inode_id, Inode &parent_inode, Entry &new_entry);
    FileSystemStatus remove_entry(int dir_inode_id, int target_inode_id);
    FileSystemStatus expand_directory(int inode_id, Inode &inode, Entry &new_entry);
    FileSystemStatus delete_file(int parent_inode_id, int inode_id);
    FileSystemStatus delete_directory(int parent_inode_id, int target_inode_id);
    /********** Path Resolution ************/
    std::expected<int, FileSystemStatus> get_inode_by_path(std::string_view path);

    /********** Bitmap (Low-Level) ************/
    std::optional<int> find_free_bit(int start_block, int total_bits);
    FileSystemStatus turn_on_bit(int bit_number, int starting_block_number, int max_bits_in_table);
    FileSystemStatus turn_off_bit(int bit_number, int starting_block_number, int max_bits_in_table);
};