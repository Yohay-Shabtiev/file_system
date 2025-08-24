#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <cstdio>
#include <vector>

#define BLOCK_SIZE 4096
#define MAX_FILE_NAME 256

struct Block
{
    int owner; // the file using the block
    char data[BLOCK_SIZE];
};

struct File
{
    int id;
    char name[MAX_FILE_NAME + 1];
    int size;
    std::vector<char *> blocks_data_pointers; // list for the beginning of the blocks the file is using
};

class FileSystem
{

private:
    int id;
    std::vector<Block> blocks;
    std::vector<int> free_blocks;
    std::vector<File> files;
    int top_free;

public:
    FileSystem(int number_of_blocks);
    ~FileSystem();
    std::vector<Block> &get_blocks();
    std::vector<File> &get_files();
    int allocate_block();
    bool free_block(int block_number);
    File *create_file(const char *name);
    bool add_data_to_file(File *f, const char *data);
    void print_meta_data(File &file) const;
};

#endif