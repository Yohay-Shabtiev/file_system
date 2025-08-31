/*
This file contains the file system class implemntation
including constructor nad methods
*/

#include "file_system.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <cstring>
#include <numeric>
#include <vector>

bool FileSystem::free_block(BlockIndex block_index)
{
    if (block_index < 0 || block_index >= free_blocks.size()) // 0 <= block_number <= free_blocks.size() - 1
        return false;

    if (top_free + 1 >= free_blocks.size()) /* The stack is already full */
        return false;

    top_free++; // top_free is points to the top block so if we add a block to the stack top++ is not exists
    free_blocks[top_free] = block_index;

    return true;
}

FileSystem::FileSystem(int number_of_blocks)
{
    id = 0;
    blocks.resize(number_of_blocks);
    free_blocks.resize(number_of_blocks);
    std::iota(free_blocks.begin(), free_blocks.end(), 0);

    top_free = number_of_blocks - 1;
}

FileSystem::~FileSystem()
{
}

std::vector<File> &FileSystem::get_files()
{
    return files;
}

std::vector<Block> &FileSystem::get_blocks()
{
    return blocks;
}

BlockIndex FileSystem::allocate_block()
{
    if (top_free < 0)
        return INVALID_BLOCK;

    BlockIndex block_index = free_blocks[top_free];
    top_free--; /* initially free_blocks[i] = i */
                /* but potentially in sometime free_block[i] might be equal to j (j != i) */

    return block_index;
}

// should add a guard for the case there is already a file with that exact name
File *FileSystem::create_file(const char *name)
{
    int name_length = strlen(name);

    if (name_length > MAX_FILE_NAME)
    {
        std::cout << "Failed to create file! (The file name is too long).\n";
        return nullptr;
    }

    files.emplace_back();
    File *f = &files.back();

    f->id = id;
    id++;

    f->name = name;

    return f;
}

bool FileSystem::add_data_to_file(File *f, const char *data)
{
    int data_length = strlen(data);
    int required_blocks = (data_length + BLOCK_SIZE - 1) / BLOCK_SIZE;

    int available_blocks = top_free + 1;
    if (available_blocks < required_blocks)
    {
        std::cout << "Not enough storage! Exiting.\n";
        return false;
    }

    for (int i = 0; i < required_blocks; i++)
    {
        BlockIndex block_index = allocate_block();
        Block &b = blocks[block_index];
        f->block_indices.push_back(block_index);
        b.owner = f->id;

        if (i < required_blocks - 1)
        {
            strncpy(b.data, data + i * BLOCK_SIZE, BLOCK_SIZE);
            continue;
        }

        int data_length_left = data_length - i * BLOCK_SIZE; // the tail's length
        strncpy(b.data, data + i * BLOCK_SIZE, data_length_left);
    }

    f->size = data_length;
    return true;
}

bool FileSystem::delete_file(int file_id)
{
    for (auto it = files.begin(); it != files.end(); ++it)
    {
        if (it->id == file_id)
        {
            it->name = "";
            it->size = 0;
            for (BlockIndex block_index : it->block_indices)
                free_block(block_index);

            files.erase(it);
            return true;
        }
    }

    return false;
}

void FileSystem::print_meta_data(const File &file) const
{
    std::cout << "\nThe file id is: %d, the name is: %s the size is: %d\n", file.id, file.name, file.size;
    std::cout << "The blocks used by the file are: ";

    int total_blocks = file.block_indices.size();

    for (int i = 0; i < total_blocks; i++)
    {
        if (i != total_blocks - 1)
            std::cout << "%d, ", file.block_indices[i];
        else
            std::cout << "%d\t", file.block_indices[i];
    }

    std::cout << "(using  totally %d blocks.)\n", total_blocks;
}

void FileSystem::print_file(const File &file) const
{
    int remaining = file.size;
    int length;

    for (BlockIndex block_index : file.block_indices)
    {
        assert(block_index >= 0 && block_index < blocks.size());

        length = std::min(remaining, BLOCK_SIZE);
        std::cout.write(blocks[block_index].data, length);
        remaining -= length;

        if (remaining == 0)
            break;
    }

    assert(remaining == 0);
    std::cout << "\n--- End of File ---\n";
}
