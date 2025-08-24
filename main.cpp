#include <stdio.h>
#include <string.h>
#include "file_system.h"

FILE *open_file(const char *file_name, const char *file_ending, const char *mode);

int main()
{
    FILE *story;
    story = open_file("peter_the_rabbit", ".txt", "r");
    if (!story)
        return 1;

    FileSystem fs = FileSystem(1000);
    char filename[MAX_FILE_NAME + 1];
    char buffer[BLOCK_SIZE + 1];
    int n, i;

    i = 0;

    while ((n = fread(buffer, sizeof(char), BLOCK_SIZE, story)) > 0)
    {
        buffer[n] = '\0';

        sprintf(filename, "Peter_The_Rabbit_Part%d.txt", i + 1); // creating the file name
        File *f = fs.create_file(filename);

        fs.add_data_to_file(f, buffer);
        i++;
    }

    if (ferror(story))
        printf("An error accured trying reading the file \"%s\"\n", filename);

    char data[BLOCK_SIZE + 1];
    for (File &file : fs.get_files())
    {
        fs.print_meta_data(file);
        for (char *c : file.blocks_data_pointers)
        {
            strcpy(data, c);
            data[strlen(c)] = '\0';
            printf("%s", data);
        }
        printf("\n--- End of File ---\n");
    }

    return 0;
}

FILE *open_file(const char *file_name, const char *file_ending, const char *mode)
{
    int name_length = strlen(file_name) + strlen(file_ending);
    char full_file_name[MAX_FILE_NAME + 1];

    strcpy(full_file_name, file_name);
    strcat(full_file_name, file_ending);
    full_file_name[name_length] = '\0';

    FILE *f = fopen(full_file_name, mode);
    if (f == NULL)
    {
        printf("Failed to open \"%s%s\"", file_name, file_ending);
    }

    return f;
}
