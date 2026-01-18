#include "constants.hpp"
#include "block_device.hpp"
#include "fs_status.hpp"
#include <string>
#include <vector>
#include <cassert>

struct Superblock
{
    int magic;
    int version;
    int total_blocks;
    int root_dir_block_index;
};

struct Inode
{
    int type;
    int size;
    int direct_block;
};

static_assert(sizeof(Inode) == 12, "Inode must be 8 bytes");

struct DirEntry
{
    int inode_id;
    int type;
    char name[ENTRY_NAME_LENGTH];
};

static_assert(sizeof(DirEntry) == 64 + 8, "padding must be different");

const int INODE_SIZE = sizeof(Inode);
const int DIR_ENTRY_SIZE = sizeof(DirEntry);

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

private:
    BlockDevice &device;
    bool is_formatted;
    Superblock superblock;
    std::vector<DirEntry> root_dir_entries;

    void load_root_dir_from_device();

    void save_root_dir_to_device();

    int allocate_inode(int type);

    FileSystemStatus write_inode(int inode_id, const Inode &inode);

    FileSystemStatus read_inode(int inode_id, Inode &inode);

    void init_inode_bitmap_on_format();

    void init_inode_blocks_on_format();

    void init_data_bitmap_on_format();

    void init_data_blocks_on_format();

    void init_root_dir_block();

    /**************  helper functions ************/
    // make sure the id is inbounds
    void validate_inode_id(int inode_id);

    FileSystemStatus allocate_data_block(int &free_block_number);

    FileSystemStatus free_data_block(int block_number);

    // this
    int inode_byte_location(int inode_id);

    bool bit_is_on(int inode_id, const int byte_index);

    // bool validate_entry_name(const std::string &entry_name);

    void set_entry_name(DirEntry &entry, const std::string &entry_name);

    int get_block_index(int inode_id);

    Inode create_inode(int type);

    FileSystemStatus create_root_dir_inode();
};
