#include "constants.hpp"
#include "block_device.hpp"
#include "fs_status.hpp"
#include <string>
#include <vector>
#include <cassert>

enum class EntryType
{
    Uninitialized = 1,
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

const int INODE_SIZE = sizeof(Inode);
const int DIR_ENTRY_SIZE = sizeof(DirEntry);
const int DIR_ENTRIES_PER_BLOCK = BLOCK_SIZE / sizeof(DirEntry);

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

    FileSystemStatus listDir(const std::string &path, std::vector<DirEntry> &entries);

    FileSystemStatus create_entry_in_root(const std::string &entry_name);

    FileSystemStatus get_root_entry_inode(const std::string &entry_name, Inode &inode);

    friend class DataManagerTest;
    friend class InodeManagerTest;

private:
    BlockDevice &device;
    bool is_formatted;
    Superblock superblock;
    std::vector<DirEntry> root_dir_entries;

    /**********     Inode    ************/
    FileSystemStatus init_inode_bitmap_on_format();
    FileSystemStatus allocate_inode(int &inode_id);
    FileSystemStatus write_inode(int inode_id, const Inode &inode);
    FileSystemStatus read_inode(int inode_id, Inode &inode);
    FileSystemStatus free_inode(int inode_id, const Inode &inode);
    FileSystemStatus create_new_inode(EntryType type, int parent_inode_id, const std::string name);
    FileSystemStatus validate_parent_and_name(int parent_inode_id, const std::string &name);
    void init_inode_blocks_on_format();

    void validate_inode_id(int inode_id);
    int inode_byte_location(int inode_id);
    // FileSystemStatus create_new_inode(int type, Inode &inode);

    /**********     Data    ************/
    FileSystemStatus init_data_bitmap_on_format();
    void init_data_blocks_on_format();
    FileSystemStatus allocate_data_block(int &free_block_number);
    FileSystemStatus free_data_block(int block_number);

    /**************  helper functions ************/
    bool bit_is_on(int inode_id, const int byte_index);
    int get_block_index(int inode_id);
    FileSystemStatus calc_next_free_dirEntry_offset(int block_number, int &offset);
    // bool validate_entry_name(const std::string &entry_name);

    /**********     Entry    ************/

    FileSystemStatus create_new_entry(DirEntry &new_entry, const std::string &new_entry_name, int new_inode_id);
    FileSystemStatus add_entry(int parent_inode_id, const std::string &name, int inode_id);
    FileSystemStatus add_entry_to_parent(int parent_inode_id, Inode &parent_inode, DirEntry &new_entry);
    int find_dirEntry(int block_number, int dirEntry_id) const;
    int block_is_full(int block_number);
    void init_data_block_to_dirEntries_block(int block_number);
    FileSystemStatus insert_dirEntry_to_block(int block_number, int dirEntry_index, DirEntry &new_entry);

    // FileSystemStatus create_new_entry(int parent_inode_id, const std::string &name, int new_inode_id, DirEntry &new_entry);

    //
    //
    //
    //
    //
    //
    //
    FileSystemStatus create_root_dir_inode();
    void set_entry_name(DirEntry &entry, const std::string &entry_name);
    void load_root_dir_from_device();
    void save_root_dir_to_device();
    void init_root_dir_block();
};
