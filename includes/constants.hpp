#pragma once
#include <cmath>

const int BITS_IN_BYTE = 8;

/* FS constants */
const int BLOCK_SIZE = 4096;
const int TOTAL_BLOCKS_NUMBER = 100;
const int ENTRY_NAME_LENGTH = 64;
const int TOTAL_INODE_NUMBER = 128;

/* Superblock constants */
const int FS_MAGIC = 0x12345678;
const int FS_VERSION = 1;

/* Reserved blocks */
const int SUPERBLOCK_INDEX = 0;
const int INODE_BITMAP_INDEX = 1;
const int DATA_BITMAP_INDEX = 2;
const int INODE_TABLE_START_INDEX = 3;
