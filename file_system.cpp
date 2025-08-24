#include <cstdio>
#include <string.h>
#include <cmath>
#include <numeric>
#include <vector>
#include "file_system.h"

class FileSystem
{

private:
    int id;
    std::vector<Block> blocks;
    std::vector<int> free_blocks;
    std::vector<File> files;
    int top_free;

    int calculate_block_index(char *ptr) const
    {
        return (ptr - this->blocks[0].data) / BLOCK_SIZE;
    }

    bool free_block(int block_number)
    {

        if (block_number < 0 || block_number >= free_blocks.size()) // 0 <= block_number <= free_blocks.size() - 1
            return false;

        if (top_free + 1 >= free_blocks.size()) /* we didn't give any block yet hence the stack is full */
            return false;

        top_free++; // top_free is points to the top block so if we add a block to the stack top++ is not exists
        free_blocks[top_free] = block_number;
        return true;
    }

public:
    FileSystem(int number_of_blocks)
    {
        id = 0;
        blocks.resize(number_of_blocks);
        free_blocks.resize(number_of_blocks);
        std::iota(free_blocks.begin(), free_blocks.end(), 0);

        top_free = number_of_blocks - 1;
    }
    ~FileSystem()
    {
    }

    std::vector<Block> &get_blocks()
    {
        return this->blocks;
    }

    std::vector<File> &get_files()
    {
        return this->files;
    }

    int allocate_block()
    {
        if (top_free < 0)
            return -1;

        int block_number = free_blocks[top_free];
        top_free--; /* initially free_blocks[i] = i */
                    /* but potentially in sometime free_block[i] might be equal to j (j != i) */

        return block_number;
    }

    // should add a guard for the case there is already a file with that exact name
    File *create_file(const char *name)
    {
        int name_length = strlen(name);

        if (name_length > MAX_FILE_NAME)
        {
            return nullptr;
        }

        this->files.emplace_back();
        File *f = &this->files.back();

        f->id = this->id;
        this->id++;

        strncpy(f->name, name, name_length);
        f->name[name_length] = '\0';

        return f;
    }

    bool add_data_to_file(File *f, const char *data)
    {
        int data_length = strlen(data);
        int required_blocks = (data_length + BLOCK_SIZE - 1) / BLOCK_SIZE;

        if (this->top_free < required_blocks)
        {
            printf("No enough storage! Exiting.\n");
            return false;
        }

        for (int i = 0; i < required_blocks; i++)
        {
            int block_index = this->allocate_block();
            Block &b = this->blocks[block_index];
            f->blocks_data_pointers.push_back(b.data);
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

    void print_meta_data(File &file) const
    {
        printf("\nThe file id is: %d, the name is: %s the size is: %d\n", file.id, file.name, file.size);
        printf("The blocks used by the file are: ");

        int total_blocks = file.blocks_data_pointers.size();

        for (int i = 0; i < total_blocks; i++)
        {
            int block_id = this->calculate_block_index(file.blocks_data_pointers[i]);
            if (i != total_blocks - 1)
                printf("%d, ", block_id);
            else
                printf("%d\t", block_id);
        }

        printf("(using  totally %d blocks.)\n", total_blocks);
    }

    bool delete_file(File file)
    {
        for (auto it = this->files.begin(); it != this->files.end(); ++it)
        {
            if (it->id == file.id)
            {
                strcpy(it->name, "");
                it->size = 0;
                for (char *c : it->blocks_data_pointers)
                {
                    int block_index = calculate_block_index(c);
                    this->free_block(block_index);
                }
                this->files.erase(it);
                return true;
            }
        }

        return false;
    }
};