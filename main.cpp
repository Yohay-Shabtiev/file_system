#include "file_system.hpp"
#include "in_memory_block_device.hpp"
#include <iostream>

int main()
{
    InMemoryBlockDevice device(1024 * 1024);
    FileSystem fs(device);
    fs.format();

    std::cout << "--- Dynamic Stress Test ---" << std::endl;

    // 1. Create a new directory
    auto dir_res = fs.create_new_inode(EntryType::Directory, 0, "home");
    if (!dir_res.has_value())
    {
        std::cout << "Failed to create /home" << std::endl;
        return 1;
    }
    int home_id = dir_res.value();
    std::cout << "[1] Created /home at Inode: " << home_id << std::endl;

    // 2. Create a file INSIDE that new directory
    auto file_res = fs.create_new_inode(EntryType::File, home_id, "notes.txt");
    if (file_res.has_value())
    {
        std::cout << "[2] Created /home/notes.txt at Inode: " << file_res.value() << std::endl;
    }

    // 3. Verify Lookup chain
    auto lookup_home = fs.lookup(0, "home");
    if (lookup_home.has_value() && lookup_home->inode_id == home_id)
    {
        auto lookup_notes = fs.lookup(home_id, "notes.txt");
        if (lookup_notes.has_value())
        {
            std::cout << "[3] Success: Full path resolution /home/notes.txt works!" << std::endl;
        }
    }

    return 0;
}