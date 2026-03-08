#pragma once

#include "constants.hpp"
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

struct DirEntry
{
    int inode_id;
    EntryType type;
    char name[ENTRY_NAME_LENGTH + 1];
};

static_assert(sizeof(DirEntry) == 64 + 8, "padding must be different");

// Global Constants for Layout
const int INODE_SIZE = sizeof(Inode);
const int DIR_ENTRY_SIZE = sizeof(DirEntry);
const int ENTRIES_PER_BLOCK = BLOCK_SIZE / sizeof(DirEntry);
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

    // The essential Public API for testing and the upcoming RPC layer
    std::expected<DirEntry, FileSystemStatus> lookup(int parent_inode_id, const std::string_view entry_name);
    std::expected<int, FileSystemStatus> create_new_inode(EntryType type, int parent_inode_id, const std::string_view name);
    std::expected<size_t, FileSystemStatus> write_file(int inode_id, std::span<const uint8_t> data, size_t offset);
    std::expected<size_t, FileSystemStatus> read_file(int inode_id, std::span<uint8_t> data, size_t offset);
    FileSystemStatus delete_file(int inode_id);

    // Testing helpers
    const std::array<int, INODE_TABLE_SIZE> &get_inode_blocks_table() const
    {
        return inode_table_blocks;
    }

    friend class DataManagerTest;
    friend class InodeManagerTest;

private:
    BlockDevice &device;
    std::array<int, INODE_TABLE_SIZE> inode_table_blocks;
    bool is_formatted;
    Superblock superblock;

    /********** Core Internal Logic (Templates) ************/

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
    void initialize_block(std::uint8_t *buffer)
    {
        T *elements = reinterpret_cast<T *>(buffer);
        T empty_element = {};
        set_as_empty(empty_element);
        std::fill(elements, elements + BLOCK_SIZE / sizeof(T), empty_element);
    }

    /********** Metadata & Initialization ************/
    void set_as_empty(DirEntry &entry);
    void set_as_empty(Inode &inode);
    Inode create_inode(EntryType type);
    DirEntry create_entry(EntryType type, int inode_id, std::string_view name);
    FileSystemStatus init_root_directory();

    /********** Inode Management ************/
    std::expected<Inode, FileSystemStatus> get_inode(int inode_id);
    std::expected<int, FileSystemStatus> allocate_inode();
    FileSystemStatus write_inode(int inode_id, const Inode &inode);
    FileSystemStatus free_inode(int inode_id);

    FileSystemStatus init_inode_bitmap_on_format();
    const FileSystemStatus init_inode_table_on_format();

    /********** Data Management ************/
    std::expected<int, FileSystemStatus> allocate_data_block();
    FileSystemStatus free_data_block(int data_block_number);
    FileSystemStatus init_data_bitmap_on_format();
    void init_data_blocks_on_format();
    std::expected<int, FileSystemStatus> get_or_allocate_block_index(int inode_id, Inode &inode, int target_block);
    std::expected<int, FileSystemStatus> FileSystem::get_block_index(Inode &inode, int target_block);

    /********** Directory & Entry Management ************/
    FileSystemStatus add_entry(int parent_inode_id, DirEntry &entry);
    FileSystemStatus add_entry_to_parent(int parent_inode_id, Inode &parent_inode, DirEntry &new_entry);
    FileSystemStatus expand_directory(int inode_id, Inode &inode, DirEntry &new_entry);

    /********** Bitwise Low-Level APIs ************/
    std::optional<int> find_free_bit(int start_block, int total_bits);
    FileSystemStatus turn_off_bit(int bit_number, int starting_block_number, int max_bits_in_table);
    FileSystemStatus turn_on_bit(int bit_number, int starting_block_number, int max_bits_in_table);
};

// #include "constants.hpp"
// #include "block_device.hpp"
// #include "fs_status.hpp"
// #include <string>
// #include <vector>
// #include <cassert>

// enum class EntryType
// {
//     Uninitialized,
//     File,
//     Directory
// };

// struct Superblock
// {
//     int magic;
//     int version;
//     int total_blocks;
//     int root_dir_block_index;
// };

// struct Inode
// {
//     EntryType type;
//     int size;
//     int direct_blocks[TOTAL_DIRECT_BLOCKS];
//     int link_count;
// };

// static_assert(sizeof(Inode) == (3 + 12) * sizeof(int), "Inode must be 60 bytes");

// struct DirEntry
// {
//     int inode_id;
//     EntryType type;
//     char name[ENTRY_NAME_LENGTH + 1];
// };

// static_assert(sizeof(DirEntry) == 64 + 8, "padding must be different");

// const int INODE_SIZE = sizeof(Inode);
// const int DIR_ENTRY_SIZE = sizeof(DirEntry);
// const int ENTRIES_PER_BLOCK = BLOCK_SIZE / sizeof(DirEntry);

// const int INODES_PER_BLOCK = BLOCK_SIZE / INODE_SIZE;
// const int INODE_TABLE_SIZE = (TOTAL_INODE_NUMBER + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK;

// const int ROOT_DIR_BLOCK_INDEX = INODE_TABLE_START_INDEX + INODE_TABLE_SIZE;

// const int DATA_START_BLOCK = ROOT_DIR_BLOCK_INDEX + 1;
// const int DATA_TABLE_SIZE = TOTAL_BLOCKS_NUMBER - DATA_START_BLOCK;

// class FileSystem
// {

// public:
//     explicit FileSystem(BlockDevice &_device);

//     void format();
//     std::expected<DirEntry, FileSystemStatus> lookup(int parent_inode_id, const std::string_view entry_name);

//     const std::array<int, INODE_TABLE_SIZE> &get_inode_blocks_table() const
//     {
//         return inode_table_blocks;
//     }

//     friend class DataManagerTest;
//     friend class InodeManagerTest;

// private:
//     BlockDevice &device;
//     std::array<int, INODE_TABLE_SIZE> inode_table_blocks;
//     bool is_formatted;
//     Superblock superblock;

//     // set out_absolute_block to the ablsolute block index
//     // returns the index of the element in the block
//     template <typename T, typename Predicate>
//     std::optional<T> get_element(
//         const int *block_indices, // the blocks where the element might be in (Inode table for inode search and  direct_blocks for DirEntry search)
//         int num_indices,          // how many blocks we need to search
//         int table_base_offset,    // the whole memory is seperate to segments - this is the segment we need to search in
//         int &out_absolute_block,  // the block_number in the whole device
//         Predicate is_match        // the lambda for finding the element
//     )
//     {
//         uint8_t buffer[BLOCK_SIZE];

//         for (int i = 0; i < num_indices; i++)
//         {
//             out_absolute_block = block_indices[i] + table_base_offset;
//             if (out_absolute_block == -1)
//                 continue;

//             auto *start = get_block_ptr<T>(out_absolute_block, buffer);
//             auto *end = start + BLOCK_SIZE / sizeof(T); // points to the first byte out of the elements segment

//             auto it = std::find_if(start, end, is_match);
//             if (it != end)
//                 return *it;
//         }

//         out_absolute_block = -1;
//         return std::nullopt;
//     }

//     /* this template returns a pointer to a cast(T) block */
//     template <typename T>
//     T *get_block_ptr(
//         int ablsolute_block_number, // the absolute block index in the deivce
//         uint8_t *buffer             // the container for the data
//     )
//     {
//         device.read_block(ablsolute_block_number, buffer);
//         return reinterpret_cast<T *>(buffer);
//     }

//     template <typename T>
//     FileSystemStatus delete_element(int absolute_block_number, int element_index)
//     {
//         // get the block to be partly deleted
//         uint8_t buffer[BLOCK_SIZE];
//         auto *elements = get_block_ptr<T>(absolute_block_number, buffer);

//         // setting a new element
//         T empty_element = {};
//         set_as_empty(empty_element);

//         // override and saving
//         elements[element_index] = empty_element;
//         return device.write_block(absolute_block_number, buffer);
//     }

//     template <typename T>
//     void initialize_block(std::uint8_t *buffer)
//     {
//         T *elements = reinterpret_cast<T *>(buffer);
//         T empty_element = {};
//         set_as_empty(empty_element);
//         std::fill(elements, elements + BLOCK_SIZE / sizeof(T), empty_element);
//     }

//     void set_as_empty(DirEntry &entry);
//     void set_as_empty(Inode &inode);

//     Inode create_inode(EntryType type);
//     DirEntry create_entry(EntryType type, int parent_inode_id, std::string_view name);
//     FileSystemStatus init_root_directory();

//     // bitwise APIs
//     std::optional<int> find_free_bit(int start, int size);

//     // from now on ingnore I now it's messy

//     // using ListCallBack = std::function<void(int, std::string_view, EntryType)>;
//     /**********     Inode    ************/
//     std::expected<Inode, FileSystemStatus> get_inode(int inode_id);
//     FileSystemStatus expand_directory(int inode_id, Inode &inode, DirEntry &new_entry);
//     FileSystemStatus init_inode_bitmap_on_format();
//     std::expected<int, FileSystemStatus> allocate_inode();
//     FileSystemStatus write_inode(int inode_id, const Inode &inode);
//     FileSystemStatus read_inode(int inode_id, Inode &inode);
//     FileSystemStatus free_inode(int inode_id);
//     std::expected<int, FileSystemStatus> create_new_inode(EntryType type, int parent_inode_id, const std::string_view name);
//     FileSystemStatus validate_parent_and_name(int parent_inode_id, const std::string_view &name);
//     const FileSystemStatus init_inode_table_on_format();

//     void validate_inode_id(int inode_id);
//     int inode_byte_location(int inode_id);
//     // FileSystemStatus create_new_inode(int type, Inode &inode);

//     /**********     Data    ************/
//     FileSystemStatus init_data_bitmap_on_format();
//     void init_data_blocks_on_format();
//     std::expected<int, FileSystemStatus> FileSystem::allocate_data_block();
//     FileSystemStatus free_data_block(int block_number);

//     /**************  helper functions ************/
//     bool bit_is_on(int inode_id, const int byte_index);
//     int get_block_index(int inode_id);
//     FileSystemStatus calc_next_free_dirEntry_offset(int block_number, int &offset);
//     // bool validate_entry_name(const std::string &entry_name);

//     /**********     Entry    ************/

//     std::optional<int> find_dirEntry(int block_number, int inode_id);

//     FileSystemStatus create_new_entry(DirEntry &new_entry, const std::string_view &new_entry_name, int new_inode_id);
//     FileSystemStatus add_entry(int parent_inode_id, DirEntry &entry);
//     FileSystemStatus add_entry_to_parent(int parent_inode_id, Inode &parent_inode, DirEntry &new_entry);
//     // int find_dirEntry(int block_number, int dirEntry_id) const;
//     int block_is_full(int block_number);
//     void init_data_block_to_dirEntries_block(int block_number);
//     FileSystemStatus insert_dirEntry_to_block(int block_number, int dirEntry_index, DirEntry &new_entry);
//     std::optional<int> find_dirEntry(int block_number, int wanted_inode_id);

//     // FileSystemStatus create_new_entry(int parent_inode_id, const std::string &name, int new_inode_id, DirEntry &new_entry);

//     //
//     //
//     //
//     //
//     //
//     //
//     //
//     FileSystemStatus create_root_dir_inode();
//     void set_entry_name(DirEntry &entry, const std::string &entry_name);
//     void load_root_dir_from_device();
//     void save_root_dir_to_device();
//     void init_root_dir_block();

//     // template <typename T>
//     // FileSystemStatus list_directory(int block_number, ListCallBack callback);
//     FileSystemStatus turn_off_bit(int bit_number, int starting_block_number, int max_bits_in_table);
//     FileSystemStatus turn_on_bit(int bit_number, int starting_block_number, int max_bits_in_table);
// };
