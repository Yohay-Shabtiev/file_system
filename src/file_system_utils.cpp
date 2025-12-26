#include "file_system.hpp"
#include "in_memory_block_device.hpp"
#include "constants.hpp"

void init_superblock(Superblock &superblock, InMemoryBlockDevice device)
{
    // init_superblock()
    superblock.magic = FS_MAGIC;
    superblock.version = FS_VERSION;
    superblock.total_blocks = device.get_total_blocks_number();
    superblock.root_dir_block_index = ROOT_DIR_BLOCK_INDEX;
};