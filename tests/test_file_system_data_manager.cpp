#include <gtest/gtest.h> // GoogleTest header
#include "in_memory_block_device.hpp"
#include "file_system.hpp"
#include "fs_status.hpp"
#include "constants.hpp"

/*
we create a class for the private APIs so we can access them
: - iniheritance in cpp
testing::Test - testing is a namespace and Test is a class
::testing.. - the :: refers to a global scope
*/
class DataManagerTest : public ::testing::Test
{
public:
    virtual ~DataManagerTest() noexcept override = default;

protected:
    FileSystemStatus call_allocate_data_block(FileSystem &fs, int &num)
    {
        return fs.allocate_data_block(num);
    }

    FileSystemStatus call_free_block(FileSystem &fs, int block_number)
    {
        return fs.free_data_block(block_number);
    }
};

TEST_F(DataManagerTest, allocate_data_block_FirstBlock)
{
    int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(size_byte);
    FileSystem fs(device);
    fs.format();

    int block_number;
    FileSystemStatus status = call_allocate_data_block(fs, block_number);

    EXPECT_EQ(block_number, 0); // since we just formatted the device
    EXPECT_EQ(status, FileSystemStatus::OK);
}

TEST_F(DataManagerTest, allocate_data_block_FullDisk)
{
    // init device and fs
    int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(size_byte);
    FileSystem fs(device);
    fs.format();

    int block_number;
    FileSystemStatus status;

    for (int i = 0; i < DATA_TABLE_SIZE; i++) // filling the device
    {
        status = call_allocate_data_block(fs, block_number);
        EXPECT_EQ(status, FileSystemStatus::OK);
    }

    status = call_allocate_data_block(fs, block_number);
    EXPECT_EQ(status, FileSystemStatus::FullDisk);
}

TEST_F(DataManagerTest, allocate_data_block_recyle_block)
{
    // init device and fs
    int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(size_byte);
    FileSystem fs(device);
    fs.format();

    int block_number;
    FileSystemStatus status;

    for (int i = 0; i < DATA_TABLE_SIZE; i++) // filling the device
    {
        status = call_allocate_data_block(fs, block_number);
        EXPECT_EQ(status, FileSystemStatus::OK);
    } // the whole data block is full

    block_number = DATA_TABLE_SIZE / 2;
    status = call_free_block(fs, block_number);
    EXPECT_EQ(status, FileSystemStatus::OK);

    int hole_block = 0;
    status = call_allocate_data_block(fs, hole_block);

    EXPECT_EQ(status, FileSystemStatus::OK);
    EXPECT_EQ(block_number, hole_block);
}