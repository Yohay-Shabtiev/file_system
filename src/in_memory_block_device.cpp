#include "in_memory_block_device.hpp"
#include <cassert>
#include <cstring>
#include "fs_status.hpp"

/*
 * This constructor allocate an in-memory blocks for the FS
 */
InMemoryBlockDevice::InMemoryBlockDevice(int size_byte)
{
    int required_blocks = (size_byte + BLOCK_SIZE - 1) / BLOCK_SIZE; // ceiling value
    if (required_blocks <= 0)
        required_blocks = 1;
    memory.resize(required_blocks);
}

int InMemoryBlockDevice::get_total_blocks_number() const
{
    return memory.size();
}

FileSystemStatus InMemoryBlockDevice::read_block(int block_index, uint8_t *buffer) const
{
    if (block_index < 0 || block_index >= get_total_blocks_number())
        return FileSystemStatus::OutOfBounds;

    std::memcpy(buffer, &memory[block_index], BLOCK_SIZE);
    return FileSystemStatus::OK;
}

FileSystemStatus InMemoryBlockDevice::write_block(int block_index, const uint8_t *buffer)
{
    if (block_index < 0 || block_index >= get_total_blocks_number())
        return FileSystemStatus::OutOfBounds;

    std::memcpy(&memory[block_index], buffer, BLOCK_SIZE);
    return FileSystemStatus::OK;
}
