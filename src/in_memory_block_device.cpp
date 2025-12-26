#include "in_memory_block_device.hpp"
#include <cassert>

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

void InMemoryBlockDevice::read_block(int block_index, std::uint8_t *buffer)
{
    assert(block_index >= 0);

    std::size_t idx = static_cast<std::size_t>(block_index);
    assert(idx < get_total_blocks_number());

    std::array<char, BLOCK_SIZE> &src = memory[idx];
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        buffer[i] = src[i];
    }
}

void InMemoryBlockDevice::write_block(int block_index, const std::uint8_t *buffer)
{
    assert(block_index >= 0);

    std::size_t idx = static_cast<std::size_t>(block_index);
    assert(idx < get_total_blocks_number());

    std::array<char, BLOCK_SIZE> &dest = memory[idx];
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        dest[i] = buffer[i];
    }
}
