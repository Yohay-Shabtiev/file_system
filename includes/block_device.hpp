/*
 * This file defines the BlockDevice interface, which provides
 * basic block I/O operations.
 *
 * It is intended to be implemented by classes such as
 * InMemoryBlockDevice and DiskMemoryBlockDevice.
 */

#pragma once
#include <cstdint>

class BlockDevice
{
public:
    virtual ~BlockDevice() = default;

    virtual int get_total_blocks_number() const = 0;

    virtual void read_block(int block_index, std::uint8_t *buffer) const = 0;

    virtual void write_block(int block_index, const std::uint8_t *buffer) = 0;
};