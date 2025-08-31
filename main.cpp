#include <cstdio>
#include <string.h>
#include "file_system.h"
#include "tests.h"

std::FILE *open_file(const char *file_name, const char *file_ending, const char *mode);

int main()
{
    FileSystem fs = FileSystem(1000); // the im-memory file system
    std::FILE *story = open_file("peter_the_rabbit", ".txt", "r");
    if (!story)
        return 1;

    break_file(fs, story, "peter_the_rabbit");

    for (const File &file : fs.get_files())
    {
        fs.print_meta_data(file);
        fs.print_file(file);
    }

    std::fclose(story);
    return 0;
}

std::FILE *open_file(const char *file_name, const char *file_ending, const char *mode)
{
    int name_length = strlen(file_name) + strlen(file_ending);
    char full_file_name[MAX_FILE_NAME + 1];

    strcpy(full_file_name, file_name);
    strcat(full_file_name, file_ending);
    full_file_name[name_length] = '\0';

    std::FILE *f = std::fopen(full_file_name, mode);
    if (f == NULL)
    {
        printf("Failed to open \"%s%s\"\n", file_name, file_ending);
    }

    return f;
}
