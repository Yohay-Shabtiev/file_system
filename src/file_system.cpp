#include "fs_constants.hpp"
#include "file_system.hpp"
#include "fs_status.hpp"
#include <cassert>
#include <vector>
#include <cstring>
#include <algorithm>
#include <string_view>
#include <optional>
#include <iostream>
#include <bit>
#include <expected>
#include <sstream>

/********************************** PUBLIC APIs **********************************/

/* ctor */
FileSystem::FileSystem(BlockDevice &_device) : device(_device), is_formatted(false)
{
    Superblock candidate;
    uint8_t buffer[BLOCK_SIZE];

    device.read_block(SUPERBLOCK_INDEX, buffer);
    std::memcpy(&candidate, buffer, sizeof(Superblock));
    int total_blocks = device.get_total_blocks_number();

    if (candidate.magic != FS_MAGIC || candidate.version != FS_VERSION)
    {
        is_formatted = false;
        return;
    }

    if (candidate.total_blocks != total_blocks)
    {
        is_formatted = false;
        return;
    }

    if (candidate.root_dir_block_index != DATA_START_BLOCK)
    {
        is_formatted = false;
        return;
    }

    // A superblock exists on the device hence the devicie was formatted by another fs
    superblock = candidate;
    is_formatted = true;
}

/*
This function copy the superblock of the FS to the
reserved block in the device
A superblock is the metadata (metadata is data about data)
the size of the storage
reserved blocks
and so on
*/
void FileSystem::format()
{
    is_formatted = true; // locate it in the end so device is locked until the initialization is over

    uint8_t buffer[BLOCK_SIZE];
    std::fill_n(buffer, BLOCK_SIZE, 0);

    // init_superblock()
    superblock.magic = FS_MAGIC;
    superblock.version = FS_VERSION;
    superblock.total_blocks = device.get_total_blocks_number();
    superblock.root_dir_block_index = DATA_START_BLOCK;

    static_assert(sizeof(Superblock) <= BLOCK_SIZE);
    Superblock *sb_block = reinterpret_cast<Superblock *>(buffer);
    sb_block[0] = superblock;

    device.write_block(SUPERBLOCK_INDEX, buffer);

    for (int i = 0; i < INODE_TABLE_SIZE; i++)
        inode_table_blocks[i] = i;

    init_inode_bitmap_on_format();
    init_inode_table_on_format();

    init_data_bitmap_on_format();
    init_data_blocks_on_format();

    init_root_directory();
}

/********** Public API ************/

std::expected<Entry, FileSystemStatus> FileSystem::lookup(int directory_inode_id, const std::string_view entry_name)
{
    auto att_res = get_attributes(directory_inode_id);

    InodeAttributes att = att_res.value();

    std::cout << "Test: the inode type is: " << int(att.type) << std::endl;
    std::cout << "Test: the size is: " << att.size << std::endl;

    if (directory_inode_id < 0 || directory_inode_id >= TOTAL_INODE_NUMBER)
        return std::unexpected(FileSystemStatus::OutOfBounds);

    // get the root inode
    auto directory_inode_res = get_inode(directory_inode_id);
    if (!directory_inode_res.has_value())
        return std::unexpected(directory_inode_res.error());

    const Inode directory_inode = directory_inode_res.value();

    int absolute_block_number;
    auto element_res = get_element<Entry>(
        directory_inode.direct_blocks,
        TOTAL_DIRECT_BLOCKS,
        0,
        absolute_block_number,
        [entry_name](const Entry &entry)
        { 
            std::cout << "Test: " << entry.name << std::endl;
            return entry.inode_id != -1 && entry.name == entry_name; });

    std::cout << "Test: " << absolute_block_number << std::endl;

    if (!element_res.has_value())
        return std::unexpected(FileSystemStatus::EntryNotFound);

    return element_res.value();
}

//************* File operations
/*
This function reads a file entry to a container given by the caller
param inode_id - the inode index to read
param data - the container sent by the caller to store the data
param offset - the first byte to read from the file data
data will store the continious memory start in offset till offset + data.size()
*/
std::expected<size_t, FileSystemStatus> FileSystem::read_file(int inode_id, std::span<uint8_t> data, size_t offset)
{
    // get the inode
    auto inode_res = get_inode(inode_id);
    if (!inode_res.has_value())
        return std::unexpected(inode_res.error());
    Inode inode = inode_res.value();

    // calc how many bytes to read
    int data_size = std::min(data.size(), inode.size - offset);
    int copied_data = 0;
    uint8_t buffer[BLOCK_SIZE];

    while (copied_data < data_size)
    {
        int current_offset = offset + copied_data;
        int target_block = current_offset / BLOCK_SIZE;
        auto source_block_res = get_block_index(inode, target_block);
        if (!source_block_res.has_value())
            return std::unexpected(source_block_res.error());

        int source_block_index = source_block_res.value();
        int starting_byte = current_offset % BLOCK_SIZE;
        int bytes_to_copy = std::min(data_size - copied_data, BLOCK_SIZE - starting_byte);

        get_block_ptr<uint8_t>(source_block_index, buffer);
        std::memcpy(data.data() + copied_data, buffer + starting_byte, bytes_to_copy);

        copied_data += bytes_to_copy;
    }

    return copied_data;
}

/*
This function writes to an entry data from a span container
param inode_id - the inode index we are writing to
param data - the data we are adding to the file
param offset - the position of the curser where data is written
The writing starts from offset byte till offset + the container size OR tha max size of a file (48KB)
*/
std::expected<size_t, FileSystemStatus> FileSystem::write_file(int inode_id, std::span<const uint8_t> data, size_t offset)
{
    // get the inode
    auto inode_res = get_inode(inode_id);
    if (!inode_res.has_value())
        return std::unexpected(inode_res.error());

    Inode inode = inode_res.value();
    int data_size = data.size();
    int written_data_size = 0;

    uint8_t buffer[BLOCK_SIZE];
    while (written_data_size < data_size) // while we still have data to write
    {
        int target_block = (offset + written_data_size) / BLOCK_SIZE;
        auto target_block_res = get_or_allocate_block_index(inode_id, inode, target_block); // getting the block to write to
        if (!target_block_res.has_value())
            return std::unexpected(inode_res.error());

        int block_index = target_block_res.value();
        int starting_byte = (offset + written_data_size) % BLOCK_SIZE;
        int required_bytes = std::min(data_size - written_data_size, BLOCK_SIZE - starting_byte);

        get_block_ptr<uint8_t>(block_index, buffer);
        std::memcpy(buffer + starting_byte, data.data() + written_data_size, required_bytes);
        device.write_block(block_index, buffer);
        written_data_size += required_bytes;
    }

    inode.size = std::max(inode.size, static_cast<int>(offset) + data_size);
    write_inode(inode_id, inode);
    return written_data_size;
}

FileSystemStatus FileSystem::delete_file(int dir_inode_id, int file_inode_id)
{
    auto file_inode_res = get_inode(file_inode_id);
    if (!file_inode_res.has_value())
        return FileSystemStatus::InodeNotFound;
    Inode file_inode = file_inode_res.value();

    FileSystemStatus status = remove_entry(dir_inode_id, file_inode_id);
    if (status != FileSystemStatus::OK)
        return status;

    if (file_inode.link_count == 1)
    {
        for (int data_block_index : file_inode.direct_blocks)
            if (data_block_index != -1)
                free_data_block(data_block_index);
        set_as_empty(file_inode);
        free_inode(file_inode_id);
        return FileSystemStatus::OK;
    }

    file_inode.link_count--;
    status = write_inode(file_inode_id, file_inode);
    if (status != FileSystemStatus::OK)
        return status;

    return FileSystemStatus::OK;
}

/*
this function creates a new file in path
*/
std::expected<int, FileSystemStatus> FileSystem::create_file(int parent_inode_id, std::string_view file_name)
{
    // auto cwd_inode_id_res = get_inode_by_path(path);
    // if (!cwd_inode_id_res)
    //     return std::unexpected(cwd_inode_id_res.error());
    // int cwd_inode_id = cwd_inode_id_res.value();

    auto new_inode_id_res = create_new_inode(EntryType::File, parent_inode_id, file_name);
    if (!new_inode_id_res)
    {
        std::cout << "the error is: " << int(new_inode_id_res.error()) << std::endl;
        return std::unexpected(new_inode_id_res.error());
    }

    return new_inode_id_res.value();
}

//************* Directory operations

std::expected<std::vector<Entry>, FileSystemStatus> FileSystem::list_directory_content(int inode_id, uint32_t block)
{
    std::vector<Entry> v_entries;

    auto inode_res = get_inode(inode_id);
    if (!inode_res.has_value())
        return std::unexpected(inode_res.error());
    Inode inode = inode_res.value();

    if (block >= TOTAL_DIRECT_BLOCKS)
        return v_entries;

    if (inode.direct_blocks[block] == -1)
        return v_entries;

    uint8_t buffer[BLOCK_SIZE];
    auto entries = get_block_ptr<Entry>(inode.direct_blocks[block], buffer);

    v_entries.insert(v_entries.end(), entries, entries + ENTRIES_PER_BLOCK);

    return v_entries;
}

FileSystemStatus FileSystem::delete_directory(int parent_inode_id, int target_inode_id)
{
    // get the target inode
    auto directory_inode_res = get_inode(target_inode_id);
    if (!directory_inode_res.has_value())
        return FileSystemStatus::InodeNotFound;
    Inode target_dir_inode = directory_inode_res.value();

    // the directory is not empty
    if (target_dir_inode.size > 2 * sizeof(Entry))
        return FileSystemStatus::InodeNotEmpty;

    FileSystemStatus status = remove_entry(parent_inode_id, target_inode_id);
    if (status != FileSystemStatus::OK)
        return status;

    // free the data blocks
    for (int block_number : target_dir_inode.direct_blocks)
        if (block_number != -1)
            free_data_block(block_number);

    // commit
    free_inode(target_inode_id);

    return FileSystemStatus::OK;
}

std::expected<int, FileSystemStatus> FileSystem::create_directory(int parent_inode_id, std::string_view directroy_name)
{
    // get the cwd directory
    // auto inode_id_res = get_inode(parent_inode_id);
    // if (!inode_id_res.has_value())
    //     return std::unexpected(inode_id_res.error());
    // int cwd_inode_id = inode_id_res.value().;

    // create a new inode for the new directory
    auto new_inode_res = create_new_inode(EntryType::Directory, parent_inode_id, directroy_name);
    if (!new_inode_res.has_value())
        return std::unexpected(new_inode_res.error());
    int new_inode_id = new_inode_res.value();

    FileSystemStatus status = init_directory_entries(new_inode_id, parent_inode_id);
    if (status != FileSystemStatus::OK)
        return std::unexpected(status);

    return new_inode_id;
}

/********************************** PRIVATE APIs **********************************/

/********** Initialization ************/

FileSystemStatus FileSystem::init_root_directory()
{
    // turn on the bit 0
    FileSystemStatus status = turn_on_bit(0, INODE_BITMAP_INDEX, TOTAL_INODE_NUMBER);
    if (status != FileSystemStatus::OK)
        return status;

    // create the inode
    Inode root_inode = create_inode(EntryType::Directory);
    status = write_inode(ROOT_INODE_ID, root_inode);
    if (status != FileSystemStatus::OK)
        return status;

    status = init_directory_entries(ROOT_INODE_ID, ROOT_INODE_ID);
    if (status != FileSystemStatus::OK)
        return status;

    return FileSystemStatus::OK;
}

FileSystemStatus FileSystem::init_directory_entries(int inode_id, int parent_inode_id)
{
    Entry dot = create_entry(EntryType::Directory, inode_id, ".");
    Entry dotDot = create_entry(EntryType::Directory, parent_inode_id, "..");

    FileSystemStatus status = add_entry(inode_id, dot);
    if (status != FileSystemStatus::OK)
        return status;

    status = add_entry(inode_id, dotDot);
    if (status != FileSystemStatus::OK)
        return status;

    return FileSystemStatus::OK;
}

FileSystemStatus FileSystem::init_inode_bitmap_on_format()
{
    uint8_t buffer[BLOCK_SIZE];
    std::fill(buffer, buffer + BLOCK_SIZE, 0xFF);

    int bytes_in_bitmap = TOTAL_INODE_NUMBER / BITS_IN_BYTE;
    if (bytes_in_bitmap > 0)
        std::fill(buffer, buffer + bytes_in_bitmap, 0x00);

    int remaining_bits = TOTAL_INODE_NUMBER % BITS_IN_BYTE;
    if (remaining_bits > 0)
    {
        uint8_t mask = static_cast<uint8_t>(0xFF << remaining_bits);
        buffer[bytes_in_bitmap] = mask;
    }

    device.write_block(INODE_BITMAP_INDEX, buffer);

    return FileSystemStatus::OK;
}

FileSystemStatus FileSystem::init_inode_table_on_format() const
{
    uint8_t buffer[BLOCK_SIZE];
    int ablsolute_block_number;

    for (int relative_block_number : get_inode_blocks_table())
    {
        std::fill(buffer, buffer + BLOCK_SIZE, 0x00);

        Inode *inodes = reinterpret_cast<Inode *>(buffer);
        for (int i = 0; i < INODES_PER_BLOCK; i++)
            for (int &block_id : inodes[i].direct_blocks)
                block_id = -1;

        ablsolute_block_number = relative_block_number + INODE_TABLE_START_INDEX;
        device.write_block(ablsolute_block_number, buffer);
    }

    return FileSystemStatus::OK;
}

FileSystemStatus FileSystem::init_data_bitmap_on_format()
{
    uint8_t buffer[BLOCK_SIZE];
    // mark anything as used to prevet access blocks that not actually exists
    std::fill_n(buffer, BLOCK_SIZE, 0xFF);
    int required_bytes = DATA_TABLE_SIZE / BITS_IN_BYTE; // full bytes to be used in the bitmap
    int remaining_bits = DATA_TABLE_SIZE % BITS_IN_BYTE; // the tail of the bitmap

    std::fill(buffer, buffer + required_bytes, 0); // mark as free

    if (remaining_bits > 0)
        buffer[required_bytes] = static_cast<uint8_t>(0xFF << remaining_bits);

    // commit changes
    device.write_block(DATA_BITMAP_INDEX, buffer);
    return FileSystemStatus::OK;
}

void FileSystem::init_data_blocks_on_format()
{
    uint8_t buffer[BLOCK_SIZE];
    std::memset(buffer, 0, BLOCK_SIZE);

    for (int i = DATA_START_BLOCK; i < TOTAL_BLOCKS_NUMBER; i++)
        device.write_block(i, buffer);
}

/********** Inode Management ************/

Inode FileSystem::create_inode(EntryType type)
{
    Inode inode;
    set_as_empty(inode);
    inode.type = type;
    inode.link_count = type == EntryType::File ? 1 : 2;
    inode.size = 0;

    return inode;
}

std::expected<Inode, FileSystemStatus> FileSystem::get_inode(int inode_id)
{
    if (inode_id < 0 || inode_id >= TOTAL_INODE_NUMBER)
        return std::unexpected(FileSystemStatus::OutOfBounds);

    uint8_t buffer[BLOCK_SIZE];
    int block_number = inode_id / INODES_PER_BLOCK;
    block_number += INODE_TABLE_START_INDEX;
    int inode_index = inode_id % INODES_PER_BLOCK;

    // auto *inodes = get_block_ptr();
    auto *inodes = get_block_ptr<Inode>(block_number, buffer);

    if (inodes[inode_index].type == EntryType::Uninitialized)
        return std::unexpected(FileSystemStatus::InodeNotFound);

    return inodes[inode_index];
}

std::expected<InodeAttributes, FileSystemStatus> FileSystem::get_attributes(int inode_id)
{
    auto inode_res = get_inode(inode_id);
    if (!inode_res.has_value())
        return std::unexpected(inode_res.error());
    Inode inode = inode_res.value();

    InodeAttributes inode_attributes;

    inode_attributes.type = inode.type;

    if (inode_attributes.type == EntryType::Directory)
        inode_attributes.size = inode.size / sizeof(Entry);
    else
        inode_attributes.size = inode.size;

    inode_attributes.link_count = inode.link_count;

    inode_attributes.blocks_used = 0;
    for (int i = 0; i < TOTAL_DIRECT_BLOCKS; i++)
        if (inode.direct_blocks[i] != -1)
            inode_attributes.blocks_used++;

    return inode_attributes;
}

// This function returns the index of the next free Inode
std::expected<int, FileSystemStatus> FileSystem::allocate_inode()
{
    // search for free inode'
    auto free_inode_res = find_free_bit(INODE_BITMAP_INDEX, TOTAL_INODE_NUMBER);
    if (!free_inode_res.has_value())
        return std::unexpected(FileSystemStatus::FullInode);

    int free_bit = free_inode_res.value();
    turn_on_bit(free_bit, INODE_BITMAP_INDEX, TOTAL_INODE_NUMBER);

    return free_bit;
}

std::expected<int, FileSystemStatus> FileSystem::create_new_inode(EntryType type, int parent_inode_id, const std::string_view name)
{
    auto inode_res = allocate_inode();
    if (!inode_res.has_value())
        return std::unexpected(inode_res.error());
    int new_inode_id = inode_res.value();

    Inode new_inode = create_inode(type);
    FileSystemStatus status = write_inode(new_inode_id, new_inode);
    if (status != FileSystemStatus::OK)
        return std::unexpected(status);

    Entry new_entry = create_entry(type, new_inode_id, name);
    status = add_entry(parent_inode_id, new_entry);
    if (status != FileSystemStatus::OK)
        return std::unexpected(status);

    return new_inode_id;
}

FileSystemStatus FileSystem::write_inode(int inode_id, const Inode &inode)
{
    if (!is_formatted)
        return FileSystemStatus::NotFormatted;

    if (inode_id < 0 || inode_id >= TOTAL_INODE_NUMBER)
        return FileSystemStatus::OutOfBounds;

    int inode_size = sizeof(Inode);
    uint8_t buffer[BLOCK_SIZE];

    int block_index = (inode_id / INODES_PER_BLOCK) + INODE_TABLE_START_INDEX;
    int block_offset = (inode_id % INODES_PER_BLOCK) * inode_size;

    device.read_block(block_index, buffer);
    std::memcpy(buffer + block_offset, &inode, inode_size);

    device.write_block(block_index, buffer);
    return FileSystemStatus::OK;
}

FileSystemStatus FileSystem::free_inode(int inode_id)
{
    if (inode_id < 0 || inode_id >= TOTAL_INODE_NUMBER)
        return FileSystemStatus::OutOfBounds;

    FileSystemStatus status = turn_off_bit(inode_id, INODE_BITMAP_INDEX, TOTAL_INODE_NUMBER);
    if (status != FileSystemStatus::OK)
        return FileSystemStatus::UnknownError;

    int block_index = inode_id / INODES_PER_BLOCK + INODE_TABLE_START_INDEX;
    int inode_index = inode_id % INODES_PER_BLOCK;

    uint8_t buffer[BLOCK_SIZE];
    auto *inodes = get_block_ptr<Inode>(block_index, buffer);
    auto &target_inode = inodes[inode_index];

    uint8_t *begin = reinterpret_cast<uint8_t *>(&target_inode); // std::fill treats a single-byte block
    uint8_t *end = begin + sizeof(Inode);
    std::fill(begin, end, 0x00);
    device.write_block(block_index, buffer);

    return FileSystemStatus::OK;
}

/********** Data Block Management ************/

/*
This method allocate the first free data block in the data table and mark the block as used
*/
std::expected<int, FileSystemStatus> FileSystem::allocate_data_block()
{
    // search a free bit in the bitmap
    auto free_bit_res = find_free_bit(DATA_BITMAP_INDEX, DATA_TABLE_SIZE);
    if (!free_bit_res.has_value())
        return std::unexpected(FileSystemStatus::FullDisk);

    // set the block as used
    int free_bit = free_bit_res.value();
    turn_on_bit(free_bit, DATA_BITMAP_INDEX, DATA_TABLE_SIZE);

    return free_bit + DATA_START_BLOCK;
}

std::expected<int, FileSystemStatus> FileSystem::get_block_index(Inode &inode, int target_block)
{
    if (target_block < 0 || target_block >= 12) // inode has at most 12 blocks
        return std::unexpected(FileSystemStatus::OutOfBounds);

    if (inode.direct_blocks[target_block] == -1) // the last block is not full
        return std::unexpected(FileSystemStatus::OutOfBounds);

    return inode.direct_blocks[target_block];
}

/*
This function returns the last partly full block or allocates a new block
param inode_id - the index of the inode
param inode - the inode of the entry
param target_block - the block index of the file blocks (0 - 11)
*/
std::expected<int, FileSystemStatus> FileSystem::get_or_allocate_block_index(int inode_id, Inode &inode, int target_block)
{
    if (inode.direct_blocks[target_block] == -1)
    {
        auto block_res = allocate_data_block();
        if (!block_res.has_value())
            return std::unexpected(block_res.error());

        int new_block_index = block_res.value();
        inode.direct_blocks[target_block] = new_block_index;
        write_inode(inode_id, inode);

        return new_block_index;
    }

    return inode.direct_blocks[target_block];
}

/*
param data_block_number - the block number in the data table itselt
*/
FileSystemStatus FileSystem::free_data_block(int data_block_number)
{
    if (data_block_number < 0 || data_block_number >= DATA_TABLE_SIZE)
        return FileSystemStatus::OutOfBounds;

    FileSystemStatus status = turn_off_bit(data_block_number, DATA_BITMAP_INDEX, DATA_TABLE_SIZE);

    if (status != FileSystemStatus::OK)
        return status;

    return FileSystemStatus::OK;
}

/********** Directory & Entry Management ************/

void FileSystem::set_as_empty(Entry &entry)
{
    entry.inode_id = -1;
    entry.type = static_cast<EntryType>(-1);
    int name_length = sizeof(entry.name) / sizeof(entry.name[0]);
    std::fill(std::begin(entry.name), std::end(entry.name), 0);
}

void FileSystem::set_as_empty(Inode &inode)
{
    inode.type = EntryType::Uninitialized;
    inode.size = 0;
    inode.link_count = 0;
    std::fill(inode.direct_blocks, inode.direct_blocks + TOTAL_DIRECT_BLOCKS, -1);
}

Entry FileSystem::create_entry(EntryType type, int inode_id, std::string_view entry_name)
{
    Entry new_entry;
    set_as_empty(new_entry);
    new_entry.type = type;
    new_entry.inode_id = inode_id;
    uint8_t entry_name_len = strlen(entry_name.data());
    strncpy(new_entry.name, entry_name.data(), entry_name_len);

    return new_entry;
}

/* this function add a new entry to an existing entry(directory) */
FileSystemStatus FileSystem::add_entry(int parent_inode_id, Entry &entry)
{
    auto parent_inode_res = get_inode(parent_inode_id);
    if (!parent_inode_res.has_value())
        return parent_inode_res.error();
    Inode parent_inode = parent_inode_res.value();

    return add_entry_to_parent(parent_inode_id, parent_inode, entry);
}

FileSystemStatus FileSystem::add_entry_to_parent(int parent_inode_id, Inode &parent_inode, Entry &new_entry)
{
    FileSystemStatus status;
    int ablsolute_block_number;
    uint8_t buffer[BLOCK_SIZE];

    // find an empty Entry slot in the parent inode
    auto Entry_res = get_element<Entry>(
        parent_inode.direct_blocks,
        TOTAL_DIRECT_BLOCKS,
        0,
        ablsolute_block_number,
        [parent_inode_id](const Entry &entry)
        { return entry.inode_id == -1; });

    if (!Entry_res.has_value())
        return expand_directory(parent_inode_id, parent_inode, new_entry);

    auto *entries = get_block_ptr<Entry>(ablsolute_block_number, buffer);
    auto *it = std::find_if(entries, entries + ENTRIES_PER_BLOCK, [](const Entry &entry)
                            { return entry.inode_id == -1; });

    // update the entries block
    int entry_index = it - entries;
    entries[entry_index] = new_entry;
    parent_inode.size += sizeof(Entry);

    // commit changes
    device.write_block(ablsolute_block_number, buffer);

    // commit changes
    write_inode(parent_inode_id, parent_inode);

    return FileSystemStatus::OK;
}

FileSystemStatus FileSystem::remove_entry(int dir_inode_id, int target_inode_id)
{
    int target_block;

    auto parent_inode_res = get_inode(dir_inode_id);
    if (!parent_inode_res.has_value())
        return parent_inode_res.error();
    Inode parent_inode = parent_inode_res.value();

    get_element<Entry>(
        parent_inode.direct_blocks,
        TOTAL_DIRECT_BLOCKS,
        0,
        target_block,
        [target_inode_id](const Entry &entry)
        { return entry.inode_id == target_inode_id; });

    if (target_block == -1)
        return FileSystemStatus::EntryNotFound;

    uint8_t buffer[BLOCK_SIZE];
    auto entries = get_block_ptr<Entry>(target_block, buffer);

    // remove the entry from the parent directory
    for (int i = 0; i < ENTRIES_PER_BLOCK; i++)
        if (entries[i].inode_id == target_inode_id)
        {
            set_as_empty(entries[i]);
            break;
        }

    parent_inode.size -= sizeof(Entry);

    // commit changes
    device.write_block(target_block, buffer);
    write_inode(dir_inode_id, parent_inode);

    return FileSystemStatus::OK;
}

FileSystemStatus FileSystem::expand_directory(int inode_id, Inode &inode, Entry &new_entry)
{
    // allocate block
    auto block_index_res = allocate_data_block();
    if (!block_index_res.has_value())
        return block_index_res.error();

    int new_block_index = block_index_res.value();

    // add the block to the inode
    bool expand_success = false;
    for (int i = 0; i < TOTAL_DIRECT_BLOCKS; i++)
    {
        if (inode.direct_blocks[i] == -1)
        {
            inode.direct_blocks[i] = new_block_index;
            expand_success = true;
            break;
        }
    }

    if (!expand_success)
        return FileSystemStatus::FullDirectory;

    // init the block
    uint8_t buffer[BLOCK_SIZE];
    initialize_block<Entry>(buffer);

    // add the entry
    auto *entries = reinterpret_cast<Entry *>(buffer);
    entries[0] = new_entry;
    inode.size += sizeof(Entry);

    // commit changes
    device.write_block(new_block_index, buffer);
    write_inode(inode_id, inode);

    return FileSystemStatus::OK;
}

FileSystemStatus FileSystem::delete_entry(int parent_inode_id, int inode_id)
{
    auto inode_res = get_inode(inode_id);
    if (!inode_res.has_value())
        return FileSystemStatus::InodeNotFound;

    if (inode_res.value().type == EntryType::File)
        return delete_file(parent_inode_id, inode_id);
    else
        return delete_directory(parent_inode_id, inode_id);
}

/********** Path Resolution ************/

/*
this function returns the inode_id of the last entry in path
*/
std::expected<int, FileSystemStatus> FileSystem::get_inode_by_path(const std::string_view path)
{
    std::stringstream s_stream(static_cast<std::string>(path));
    std::string entry_name;

    Entry entry;

    int current_inode_id = ROOT_INODE_ID; // starting from root directory
    while (std::getline(s_stream, entry_name, '/'))
    {
        if (entry_name.empty())
            continue;

        auto entry_res = lookup(current_inode_id, entry_name);
        if (!entry_res.has_value())
            return std::unexpected(entry_res.error());

        entry = entry_res.value();
        current_inode_id = entry.inode_id;
    }

    return current_inode_id;
}

/********** Bitmap (Low-Level) ************/

std::optional<int> FileSystem::find_free_bit(int start_block, int total_bits)
{
    uint8_t buffer[BLOCK_SIZE];

    int free_bit;
    const int bits_per_block = BLOCK_SIZE * BITS_IN_BYTE;

    int bitmap_size = (total_bits + bits_per_block - 1) / bits_per_block;
    for (int j = 0; j < bitmap_size; j++)
    {
        auto *bytes = get_block_ptr<uint8_t>(start_block + j, buffer);
        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if (bytes[i] == 0xFF)
                continue;

            free_bit = std::countr_one(bytes[i]); // return the index of the first 0

            int absolute_free_bit = (j * BLOCK_SIZE * BITS_IN_BYTE) + i * BITS_IN_BYTE + free_bit;
            if (absolute_free_bit < total_bits)
                return absolute_free_bit;
        }
    }

    return std::nullopt;
}

FileSystemStatus FileSystem::turn_on_bit(int bit_number, int starting_block_number, int max_bits_in_table)
{
    if (bit_number < 0 || bit_number >= max_bits_in_table)
        return FileSystemStatus::OutOfBounds;

    int absolute_block_index;

    int table_block_index = bit_number / (BLOCK_SIZE * BITS_IN_BYTE);
    int byte_index = bit_number / BITS_IN_BYTE;
    byte_index %= BLOCK_SIZE;
    int bit_index = bit_number % BITS_IN_BYTE;

    uint8_t mask = static_cast<uint8_t>(1 << bit_index);
    absolute_block_index = table_block_index + starting_block_number;

    uint8_t buffer[BLOCK_SIZE];

    device.read_block(absolute_block_index, buffer);
    buffer[byte_index] |= mask;
    device.write_block(absolute_block_index, buffer);

    return FileSystemStatus::OK;
}

/*
this function mark an element as free
bit_number - The global index of the bit to be turn off to 0
starting_block_number - the first block index of the bitmap table
max_bits_in_table - the total bits in the bitmap
*/
FileSystemStatus FileSystem::turn_off_bit(int bit_number, int starting_block_number, int max_bits_in_table)
{
    if (bit_number < 0 || bit_number >= max_bits_in_table)
        return FileSystemStatus::OutOfBounds;

    int block_index = bit_number / (BLOCK_SIZE * BITS_IN_BYTE); // the block in the table
    int byte_number = bit_number / BITS_IN_BYTE;                // the byte index in the whole table
    int byte_index = byte_number % BLOCK_SIZE;                  // the byte index in the block
    int bit_index = bit_number % BITS_IN_BYTE;                  // the bit in the byte

    uint8_t buffer[BLOCK_SIZE];
    device.read_block(starting_block_number + block_index, buffer);

    uint8_t mask = 1 << bit_index;
    buffer[byte_index] &= ~mask;
    device.write_block(starting_block_number + block_index, buffer);

    return FileSystemStatus::OK;
}
