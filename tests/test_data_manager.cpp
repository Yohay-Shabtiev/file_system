#include <gtest/gtest.h>
#include "in_memory_block_device.hpp"
#include "file_system.hpp"
#include "fs_status.hpp"
#include "fs_constants.hpp"

class DataManagerTest : public ::testing::Test
{
public:
    virtual ~DataManagerTest() noexcept override = default;

protected:
    FileSystemStatus call_init_data_bitmap_on_format(FileSystem &fs)
    {
        return fs.init_data_bitmap_on_format();
    }

    std::expected<int, FileSystemStatus> call_allocate_data_block(FileSystem &fs)
    {
        return fs.allocate_data_block();
    }

    FileSystemStatus call_free_block(FileSystem &fs, int block_number)
    {
        return fs.free_data_block(block_number);
    }
};

TEST_F(DataManagerTest, init_data_bitmap)
{
    // todo
}

TEST_F(DataManagerTest, allocate_data_block_FirstBlock)
{
    int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(size_byte);
    FileSystem fs(device);
    call_init_data_bitmap_on_format(fs); // bitmap only — no blocks pre-allocated

    auto result = call_allocate_data_block(fs);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), DATA_START_BLOCK); // first absolute data block
}

TEST_F(DataManagerTest, allocate_data_block_FullDisk)
{
    int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(size_byte);
    FileSystem fs(device);
    call_init_data_bitmap_on_format(fs);

    for (int i = 0; i < DATA_TABLE_SIZE; i++)
    {
        auto result = call_allocate_data_block(fs);
        EXPECT_TRUE(result.has_value());
    }

    auto result = call_allocate_data_block(fs);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FileSystemStatus::FullDisk);
}

TEST_F(DataManagerTest, allocate_data_block_recyle_block)
{
    int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(size_byte);
    FileSystem fs(device);
    call_init_data_bitmap_on_format(fs);

    for (int i = 0; i < DATA_TABLE_SIZE; i++)
    {
        auto result = call_allocate_data_block(fs);
        EXPECT_TRUE(result.has_value());
    }

    int relative_block = DATA_TABLE_SIZE / 2;
    FileSystemStatus status = call_free_block(fs, relative_block);
    EXPECT_EQ(status, FileSystemStatus::OK);

    auto result = call_allocate_data_block(fs);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), relative_block + DATA_START_BLOCK);
}
