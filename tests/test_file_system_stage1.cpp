/*
THIS FILE TESTS THE FFUNCTIONALITY OF InMemoryBlockDevice
*/

#include <gtest/gtest.h> // GoogleTest header
#include "in_memory_block_device.hpp"
#include "constants.hpp"

TEST(InMemoryBlockDevice, total_blocks)
{
    int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(size_byte);

    EXPECT_EQ(TOTAL_BLOCKS_NUMBER, device.get_total_blocks_number());
}

TEST(InMemoryBlockDevice, read_write_block)
{
    int size_byte = BLOCK_SIZE * TOTAL_BLOCKS_NUMBER;
    InMemoryBlockDevice device(size_byte);

    int total_blocks = device.get_total_blocks_number();
    assert(device.get_total_blocks_number() >= 2);

    std::uint8_t buffer1[BLOCK_SIZE];
    std::uint8_t buffer2[BLOCK_SIZE];

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        buffer1[i] = 1;
        buffer2[i] = 2;
    }

    device.write_block(total_blocks - 1, buffer1);
    device.write_block(total_blocks - 2, buffer2);

    device.read_block(total_blocks - 2, buffer1);

    for (int i = 0; i < BLOCK_SIZE; i++)
        EXPECT_EQ(buffer1[i], buffer2[i]);
}