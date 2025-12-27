#pragma once

#include "constants.hpp"
#include "block_device.hpp"
#include <vector>
#include <array>
#include <cstdint>

class InMemoryBlockDevice : public BlockDevice
{
private:
    std::vector<std::array<std::uint8_t, BLOCK_SIZE>> memory;

public:
    /* constructs a memory block */
    explicit InMemoryBlockDevice(int size_byte);

    int get_total_blocks_number() const override;
    void read_block(int block_index, std::uint8_t *buffer) const override;
    void write_block(int block_index, const std::uint8_t *buffer) override;
};