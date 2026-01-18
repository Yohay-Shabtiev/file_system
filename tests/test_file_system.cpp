#include <gtest/gtest.h> // GoogleTest header
#include "in_memory_block_device.hpp"
#include "file_system.hpp"
#include "fs_status.hpp"
#include "constants.hpp"

#include <vector>
#include <string>
#include <cstring>

/******************* listDir TESTs *******************/

/******************* CTOR TESTs *******************/

TEST_FS(FileSystemTest, ctorDeviceIsNotFormatted)
{
    int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(size_byte);
    FileSystem fs(device);
    std::string root_path = "/";
    std::vector<DirEntry> entries;

    FileSystemStatus status;
    status = fs.listDir(root_path, entries);
    EXPECT_EQ(status, FileSystemStatus::NotFormatted);
}

TEST_FS(FileSystemTest, ctorDeviceRemount)
{
    int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(size_byte);
    {
        FileSystem fs1(device);
        fs1.format();
    }

    FileSystem fs2(device);

    std::string root_path = "/";
    std::vector<DirEntry> entries;
    FileSystemStatus status = fs2.listDir(root_path, entries);

    EXPECT_TRUE(entries.empty());
    EXPECT_EQ(status, FileSystemStatus::OK);
}
// TEST_FS(FileSystemTest, corruptSuperblock)
// {
//     int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
//     InMemoryBlockDevice device(size_byte);
//     FileSystem fs(device);
//     fs.format();

//     std::string root_path = "/";
//     std::vector<DirEntry> entries;

//     std::uint8_t buffer[BLOCK_SIZE];
//     Superblock sb;

//     device.read_block(SUPERBLOCK_INDEX, buffer);
//     std::memcpy(&sb, buffer, sizeof(Superblock));

//     sb.magic++;
//     std::memcpy(buffer, &sb, sizeof(Superblock));
//     device.write_block(SUPERBLOCK_INDEX, buffer);

//     FileSystem fs2(device);
//     FileSystemStatus status = fs2.listDir(root_path, entries);

//     EXPECT_EQ(status, FileSystemStatus::NotFormatted);
// }

/******************* FORMAT TESTs *******************/

// This test checks formatted correctly

// This test checks what if we modify the superblock after format

TEST_FS(FileSystemTest, formatInitializeBitmap)
{
    int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(size_byte);
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

TEST_FS(FileSystemTest, formatInitializeInodeBlocks)
{
    int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(size_byte);
    FileSystem fs(device);
    fs.format();

    std::uint8_t buffer[BLOCK_SIZE];
    int start = INODE_TABLE_START_INDEX;
    int end = INODE_TABLE_SIZE + INODE_TABLE_START_INDEX;

    for (int i = start; i < end; i++)
    {
        device.read_block(i, buffer);
        for (int j = 0; j < BLOCK_SIZE; j++)
            EXPECT_EQ(buffer[j], 0);
    }
}

TEST_FS(FileSystemTest, formatRemount)
{
    int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(size_byte);
    {
        FileSystem fs(device);
        fs.format();
    }
}

// /******************* CREATE_ENTY_IN_ROOT TESTs *******************/

// TEST_FS(FileSystemTest, CreateEntryInRootNotFormatted)
// {
//     int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
//     InMemoryBlockDevice device(size_byte);
//     FileSystem fs(device);

//     std::string s = "my_entry";
//     FileSystemStatus status = fs.create_entry_in_root(s);

//     EXPECT_EQ(status, FileSystemStatus::NotFormatted);
// }

// TEST_FS(FileSystemTest, CreateEntryInRootNoName)
// {
//     int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
//     InMemoryBlockDevice device(size_byte);
//     FileSystem fs(device);

//     fs.format();

//     std::string s = "";
//     FileSystemStatus status = fs.create_entry_in_root(s);

//     EXPECT_EQ(status, FileSystemStatus::UnknownError);
// }

// TEST_FS(FileSystemTest, CreateEntryInRootTooLongName)
// {
//     int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
//     InMemoryBlockDevice device(size_byte);
//     FileSystem fs(device);

//     fs.format();

//     std::string s = "a";
//     for (int i = 0; i < ENTRY_NAME_LENGTH; i++)
//         s = s + "a";
//     FileSystemStatus status = fs.create_entry_in_root(s);

//     EXPECT_EQ(status, FileSystemStatus::UnknownError);
// }

// TEST_FS(FileSystemTest, CreateEntryInRootNoSpaceOnDisk)
// {
//     int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
//     InMemoryBlockDevice device(size_byte);
//     FileSystem fs(device);

//     fs.format();

//     FileSystemStatus status;
//     std::string s;
//     for (int i = 0; i < MAX_ROOT_ENTRIES; i++)
//     {
//         s = "f" + std::to_string(i);
//         status = fs.create_entry_in_root(s);
//         EXPECT_EQ(status, FileSystemStatus::OK);
//     }

//     status = fs.create_entry_in_root("b");
//     EXPECT_EQ(status, FileSystemStatus::UnknownError);
// }

// TEST_FS(FileSystemTest, CreateEntryInRootDuplicateName)
// {
//     int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
//     InMemoryBlockDevice device(size_byte);
//     FileSystem fs(device);

//     fs.format();
//     FileSystemStatus status;

//     status = fs.create_entry_in_root("a");
//     EXPECT_EQ(status, FileSystemStatus::OK);

//     status = fs.create_entry_in_root("a");
//     EXPECT_EQ(status, FileSystemStatus::UnknownError);
// }

// TEST_FS(FileSystemTest, CreateEntryInRootDirRemount)
// {
//     int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
//     InMemoryBlockDevice device(size_byte);
//     FileSystem fs1(device);

//     fs1.format();

//     FileSystemStatus status;
//     status = fs1.create_entry_in_root("f");
//     EXPECT_EQ(status, FileSystemStatus::OK);

//     FileSystem fs2(device);
//     status = fs2.create_entry_in_root("f");
//     EXPECT_EQ(status, FileSystemStatus::UnknownError);
// }

// TEST_FS(FileSystemTest, CreateEntryInRootPersistsAndListedAfterRemount)
// {
//     // TODO
// }

// TEST_FS(FileSystemTest, createEntryInRootRemountInodeExistance)
// {
//     int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
//     InMemoryBlockDevice device(size_byte);

//     {
//         FileSystem fs1(device);
//         fs1.format();
//         fs1.create_entry_in_root("f");
//     }

//     FileSystem fs2(device);

//     Inode inode;
//     FileSystemStatus st = fs2.get_root_entry_inode("f", inode);
//     EXPECT_EQ(st, FileSystemStatus::OK);
//     EXPECT_EQ(inode.type, 1);
//     EXPECT_EQ(inode.size, 0);
// }

// // this are new
// TEST_FS(FileSystemTest, create_entry_in_root_NotFormatted)
// {
//     int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
//     InMemoryBlockDevice device(size_byte);
//     FileSystem fs(device);

//     fs.create_entry_in_root("f");

//     Inode inode;
//     FileSystemStatus st = fs.get_root_entry_inode("f", inode);

//     EXPECT_EQ(st, FileSystemStatus::NotFormatted);
// }

// TEST_FS(FileSystemTest, get_root_entry_inode_NotFound)
// {
//     int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
//     InMemoryBlockDevice device(size_byte);
//     FileSystem fs(device);
//     fs.format();

//     FileSystemStatus st = fs.create_entry_in_root("f");
//     EXPECT_EQ(st, FileSystemStatus::OK);

//     Inode inode;
//     st = fs.get_root_entry_inode("a", inode);

//     EXPECT_EQ(st, FileSystemStatus::NotFormatted);
// }

// TEST_FS(FileSystemTest, create_entry_in_root_disk_is_full)
// {
//     int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
//     InMemoryBlockDevice device(size_byte);
//     FileSystem fs(device);

//     {
//         fs.format();
//         FileSystemStatus st = fs.create_entry_in_root("f1");
//         EXPECT_EQ(st, FileSystemStatus::OK);
//     }

//     FileSystem fs2(device);
//     FileSystemStatus st = fs2.create_entry_in_root("f2");
//     EXPECT_EQ(st, FileSystemStatus::OK);

//     Inode inode;
//     for (int i = 0; i < TOTAL_INODE_NUMBER; i++)
//     {
//         std::string entry_name = "f" + std::to_string(i);
//         st = fs2.get_root_entry_inode(entry_name, inode);
//         EXPECT_EQ(inode.type, 1);
//         EXPECT_EQ(inode.size, 0);
//     }

//     st = fs.create_entry_in_root("a");
//     EXPECT_EQ(st, FileSystemStatus::UnknownError);
// }
