#include <gtest/gtest.h> // GoogleTest header
#include "in_memory_block_device.hpp"
#include "file_system.hpp"
#include "fs_status.hpp"
#include "constants.hpp"

#include <vector>
#include <string>
#include <cstring>

/******************* listDir TESTs *******************/

TEST(FileSystemTest, listDir_NotFormatted)
{
    int byte_size = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(byte_size);

    FileSystem fs(device);
    std::vector<DirEntry> entries;

    FileSystemStatus status = fs.listDir("/", entries);
    EXPECT_EQ(status, FileSystemStatus::NotFormatted);
}

TEST(FileSystemTest, listDir_NotFound)
{
    int byte_size = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(byte_size);
    FileSystem fs(device);
    fs.format();

    std::string invalid_root_path = "/A";
    std::vector<DirEntry> entries;

    FileSystemStatus status = fs.listDir(invalid_root_path, entries);
    EXPECT_EQ(status, FileSystemStatus::NotFound);
}

TEST(FileSystemTest, listDir_NotFound_and_NotFormatted)
{
    int byte_size = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(byte_size);
    FileSystem fs(device);

    std::string invalid_root_path = "/A";
    std::vector<DirEntry> entries;

    FileSystemStatus status = fs.listDir(invalid_root_path, entries);

    EXPECT_EQ(status, FileSystemStatus::NotFormatted);
}

TEST(FileSystemTest, listDir_OK)
{
    int byte_size = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(byte_size);

    FileSystem fs(device);
    fs.format();

    std::string valid_path = "/";
    std::vector<DirEntry> entries;

    FileSystemStatus status = fs.listDir(valid_path, entries);
    EXPECT_TRUE(entries.empty());
    EXPECT_EQ(status, FileSystemStatus::OK);
}

TEST(FileSystemTest, listDir_remount)
{
    int byte_size = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(byte_size);

    {
        FileSystem fs(device);
        fs.format();
    }

    FileSystem fs2(device);
    std::vector<DirEntry> entries;

    FileSystemStatus st = fs2.listDir("F", entries);
    EXPECT_EQ(st, FileSystemStatus::NotFound);

    FileSystemStatus st = fs2.listDir("/", entries);

    EXPECT_TRUE(entries.empty());
    EXPECT_EQ(st, FileSystemStatus::OK);
}

/******************* FORMAT TESTs *******************/

TEST(FileSystemTest, formatWritesSuperblock)
{
    int byte_size = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(byte_size);
    FileSystem fs(device);
    fs.format();

    int total_blocks = device.get_total_blocks_number();

    Superblock superblock;
    std::uint8_t buffer[BLOCK_SIZE];
    device.read_block(SUPERBLOCK_INDEX, buffer);
    std::memcpy(&superblock, buffer, sizeof(Superblock));

    EXPECT_EQ(superblock.magic, FS_MAGIC);
    EXPECT_EQ(superblock.version, FS_VERSION);
    EXPECT_EQ(superblock.root_dir_block_index, DATA_START_BLOCK);
    EXPECT_EQ(superblock.total_blocks, total_blocks);
}

TEST(FileSystemTest, corruptSuperblock)
{
    int byte_size = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(byte_size);
    std::string root_path = "/";
    std::vector<DirEntry> entries;

    {
        FileSystem fs(device);
        fs.format();

        std::uint8_t buffer[BLOCK_SIZE];
        Superblock sb;

        device.read_block(SUPERBLOCK_INDEX, buffer);
        std::memcpy(&sb, buffer, sizeof(Superblock));

        sb.magic++;
        std::memcpy(buffer, &sb, sizeof(Superblock));
        device.write_block(SUPERBLOCK_INDEX, buffer);
    }

    FileSystem fs2(device);
    FileSystemStatus status = fs2.listDir(root_path, entries);

    EXPECT_EQ(status, FileSystemStatus::NotFormatted);
}

TEST(FileSystemTest, format_initialize_Bitmap)
{
    int byte_size = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(byte_size);
    FileSystem fs(device);
    fs.format();   

    std::uint8_t buffer[BLOCK_SIZE];
    device.read_block(INODE_BITMAP_INDEX, buffer);

    int bytes_in_bitmap = (TOTAL_INODE_NUMBER + BITS_IN_BYTE - 1) / BITS_IN_BYTE;

    for (int i = 0; i < bytes_in_bitmap; i++) // all bits
        EXPECT_EQ(buffer[i], 0);

    for (int i = bytes_in_bitmap; i < BLOCK_SIZE; i++)
        EXPECT_EQ(buffer[i], 0xFF);
}