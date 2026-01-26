#include <gtest/gtest.h> // GoogleTest header
#include "in_memory_block_device.hpp"
#include "file_system.hpp"
#include "fs_status.hpp"
#include "constants.hpp"

class InodeManagerTest : public ::testing::Test
{

public:
    virtual ~InodeManagerTest() noexcept override = default;

protected:
    FileSystemStatus call_init_inode_bitmap_on_format(FileSystem &fs)
    {
        return fs.init_inode_bitmap_on_format();
    }
    FileSystemStatus call_allocate_inode(FileSystem &fs, int &inode_id)
    {
        return fs.allocate_inode(inode_id);
    }
    FileSystemStatus call_write_inode(FileSystem &fs, int inode_id, const Inode &inode)
    {
        return fs.write_inode(inode_id, inode);
    }
    FileSystemStatus call_read_inode(FileSystem &fs, int inode_id, Inode &inode)
    {
        return fs.read_inode(inode_id, inode);
    }
};

TEST_F(InodeManagerTest, init_inode_bitmap)
{
    int size_byte = TOTAL_BLOCKS_NUMBER * BLOCK_SIZE;
    InMemoryBlockDevice device(size_byte);
    FileSystem fs(device);

    FileSystemStatus status = call_init_inode_bitmap_on_format(fs);
    EXPECT_EQ(status, FileSystemStatus::OK);

    std::uint8_t buffer[BLOCK_SIZE];
    device.read_block(INODE_BITMAP_INDEX, buffer);

    int total_bytes = TOTAL_INODE_NUMBER / BITS_IN_BYTE;
    for (int i = 0; i < total_bytes; i++)
        EXPECT_EQ(buffer[i], 0);

    int unused_first_byte;
    int remain_bits = TOTAL_INODE_NUMBER % BITS_IN_BYTE;
    if (remain_bits == 0)
        unused_first_byte = total_bytes;
    else
    {
        std::uint8_t mask = 0xFF;
        std::uint8_t unused_bits = mask << remain_bits;
        std::uint8_t used_bits = ~unused_bits;

        EXPECT_EQ(buffer[total_bytes] & unused_bits, unused_bits);
        EXPECT_EQ(buffer[total_bytes] & used_bits, 0);
        unused_first_byte = total_bytes + 1;
    }

    for (int i = unused_first_byte; i < BLOCK_SIZE; i++)
        EXPECT_EQ(buffer[i], 0xFF);
};

TEST_F(InodeManagerTest, allocate_inode)
{
    int size_byte = TOTAL_BLOCKS_NUMBER * BLOCK_SIZE;
    InMemoryBlockDevice device(size_byte);
    FileSystem fs(device);
    call_init_inode_bitmap_on_format(fs);

    int inode_id;
    FileSystemStatus status;
    for (int i = 0; i < TOTAL_INODE_NUMBER; i++)
    {
        status = call_allocate_inode(fs, inode_id);
        EXPECT_EQ(status, FileSystemStatus::OK);
        EXPECT_EQ(inode_id, i);
    }

    status = call_allocate_inode(fs, inode_id);
    EXPECT_EQ(status, FileSystemStatus::OK);
};

TEST_F(InodeManagerTest, full_inode_table)
{
    int size_byte = TOTAL_BLOCKS_NUMBER * BLOCK_SIZE;
    InMemoryBlockDevice device(size_byte);
    FileSystem fs(device);
    call_init_inode_bitmap_on_format(fs);

    int inode_id;
    for (int i = 0; i < TOTAL_INODE_NUMBER; i++)
        call_allocate_inode(fs, inode_id);

    FileSystemStatus status = call_allocate_inode(fs, inode_id);
    EXPECT_EQ(status, FileSystemStatus::FullInode);
};

TEST_F(InodeManagerTest, write_inode){

};
TEST_F(InodeManagerTest, read_inode){

};
TEST_F(InodeManagerTest, recycle_inode){

};
