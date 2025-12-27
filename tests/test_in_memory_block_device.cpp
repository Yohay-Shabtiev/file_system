#include <gtest/gtest.h> // GoogleTest header
#include "in_memory_block_device.hpp"


TEST(InMemoryBlockDeviceTest, readWriteTest)
{
    // Arrange: create a device with some size
    int size_bytes = 15000;
    InMemoryBlockDevice dev(size_bytes); // construct a memory device

    char read_buffer[BLOCK_SIZE];
    char write_buffer[BLOCK_SIZE];

    // init the buffers to be different
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        read_buffer[i] = 'A';
        write_buffer[i] = 'B';
    }

    dev.write_block(0, write_buffer); // writing to the device block
    dev.read_block(0, read_buffer);   // reading from the device block

    for (int i; i < BLOCK_SIZE; i++)
    {
        EXPECT_EQ(read_buffer[i], write_buffer[i]);
    }
}
