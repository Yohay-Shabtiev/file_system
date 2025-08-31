/*
This file contains the file system class decleration
Any constants used in the program
*/

#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <cstdio>
#include <string>
#include <vector>

#define BLOCK_SIZE 4096
#define MAX_FILE_NAME 256

using BlockIndex = int;
enum : int
{
    INVALID_BLOCK = -1
};

struct Block
{
    int owner; // the file using the block
    char data[BLOCK_SIZE];
};

struct File
{
    int id;
    std::string name;
    int size;
    std::vector<BlockIndex> block_indices;
};

class FileSystem
{

private:
    int id;
    std::vector<Block> blocks;
    std::vector<BlockIndex> free_blocks;
    std::vector<File> files;
    int top_free;

    bool free_block(BlockIndex block_number);

public:
    FileSystem(int number_of_blocks);
    ~FileSystem();

    std::vector<File> &get_files();
    std::vector<Block> &get_blocks();

    BlockIndex allocate_block();

    File *create_file(const char *name);
    bool add_data_to_file(File *f, const char *data);
    bool delete_file(int file_id);

    void print_meta_data(const File &file) const;
    void print_file(const File &file) const;
};

#endif