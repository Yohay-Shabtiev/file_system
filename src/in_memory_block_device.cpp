#include "in_memory_block_device.hpp"
#include <cassert>
#include <cstring>

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

void InMemoryBlockDevice::read_block(int block_index, std::uint8_t *buffer) const
{
    assert(block_index >= 0);
    assert(block_index < get_total_blocks_number());

    std::memcpy(buffer, &memory[block_index], BLOCK_SIZE);
}

void InMemoryBlockDevice::write_block(int block_index, const std::uint8_t *buffer)
{
    assert(block_index >= 0);
    assert(block_index < get_total_blocks_number());

    std::memcpy(&memory[block_index], buffer, BLOCK_SIZE);
}
