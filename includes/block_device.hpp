/*
 * This file defines the BlockDevice interface, which provides
 * basic block I/O operations.
 *
 * It is intended to be implemented by classes such as
 * InMemoryBlockDevice and DiskMemoryBlockDevice.
 */

#pragma once
#include <cstdint>
#include "fs_status.hpp"

class BlockDevice
{
public:
    virtual ~BlockDevice() = default;

    virtual int get_total_blocks_number() const = 0;

    virtual FileSystemStatus read_block(int block_index, uint8_t *buffer) const = 0;

    virtual FileSystemStatus write_block(int block_index, const uint8_t *buffer) = 0;
};