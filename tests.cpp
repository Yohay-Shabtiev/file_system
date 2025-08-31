#include "file_system.h"

/* this file breaks a multi-blocks file to single-block files */
bool break_file(FileSystem &fs, FILE *file, const char *filename)
{
    int n; // how many tokens were read from the file
    char buffer[BLOCK_SIZE + 1];

    int i = 1;
    while ((n = std::fread(buffer, sizeof(char), BLOCK_SIZE, file)) > 0)
    {
        buffer[n] = '\0';

        std::string curr_filename = "Peter_The_Rabbit_Part" + std::to_string(i) + ".txt";
        File *f = fs.create_file(curr_filename.c_str());

        fs.add_data_to_file(f, buffer);
        i++;
    }

    if (ferror(file))
    {
        printf("An error accured trying reading the file: \"%s\"\n", filename);
        return false;
    }

    return true;
}