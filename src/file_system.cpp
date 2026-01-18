#include "constants.hpp"
#include "file_system.hpp"
#include "fs_status.hpp"
#include <cassert>
#include <vector>
#include <cstring>
#include <algorithm>

void init_dirEntry(DirEntry &dirEntry);
/********************************** PUBLIC APIs **********************************/

/* ctor */
FileSystem::FileSystem(BlockDevice &_device) : device(_device), is_formatted(false)
{
    Superblock candidate;
    std::uint8_t buffer[BLOCK_SIZE];

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
    load_root_dir_from_device();
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
    std::uint8_t buffer[BLOCK_SIZE];
    std::memset(buffer, 0, BLOCK_SIZE);

    // init_superblock()
    superblock.magic = FS_MAGIC;
    superblock.version = FS_VERSION;
    superblock.total_blocks = device.get_total_blocks_number();
    superblock.root_dir_block_index = DATA_START_BLOCK;

    static_assert(sizeof(Superblock) <= BLOCK_SIZE);
    std::memcpy(buffer, &superblock, sizeof(Superblock));
    device.write_block(SUPERBLOCK_INDEX, buffer);

    is_formatted = true;
    root_dir_entries.clear();

    init_inode_bitmap_on_format();
    init_inode_blocks_on_format();

    init_data_bitmap_on_format();
    init_data_blocks_on_format();

    init_root_dir_block();
    // create_root_dir_inode();
    // save_root_dir_to_device();
}

/* this function initialize the inode bitmap block
   once we allocate an inode we turn on the bit (swith to 1)
   til then the bit will be off (0)
   since there are only 128 inodes in the FS all the overflow bits
   will be set to '1' since we are not suppose to allocate them anyway
*/
void FileSystem::init_root_dir_block()
{
    std::uint8_t buffer[BLOCK_SIZE];
    std::memset(buffer, 0, BLOCK_SIZE);

    DirEntry de;
    init_dirEntry(de);

    int total_dirEntries_per_block = BLOCK_SIZE / DIR_ENTRY_SIZE;
    for (int i = 0; i < total_dirEntries_per_block; i++)
    {
        std::memcpy(buffer + i * DIR_ENTRY_SIZE, &de, DIR_ENTRY_SIZE);
    }

    device.write_block(ROOT_DIR_BLOCK_INDEX, buffer);
}

void init_dirEntry(DirEntry &dirEntry)
{
    dirEntry.inode_id = -1;
    dirEntry.name[0] = '\0';
    dirEntry.type = -1;
}

void FileSystem::init_inode_bitmap_on_format()
{
    std::uint8_t buffer[BLOCK_SIZE];
    std::memset(buffer, 0xFF, BLOCK_SIZE);

    int bytes_in_bitmap = TOTAL_INODE_NUMBER / BITS_IN_BYTE;

    if (bytes_in_bitmap > 0)
        std::memset(buffer, 0, bytes_in_bitmap);

    int remaining_bits = TOTAL_INODE_NUMBER % BITS_IN_BYTE;
    if (remaining_bits > 0)
    {
        std::uint8_t mask = 0xFF;
        mask <<= remaining_bits;
        buffer[bytes_in_bitmap] = mask;
    }

    device.write_block(INODE_BITMAP_INDEX, buffer);
}

void FileSystem::init_inode_blocks_on_format()
{
    std::uint8_t buffer[BLOCK_SIZE];
    std::memset(buffer, 0, BLOCK_SIZE);

    int start = INODE_TABLE_START_INDEX;
    int end = INODE_TABLE_SIZE + INODE_TABLE_START_INDEX;

    for (int i = start; i < end; i++)
        device.write_block(i, buffer);
}

void FileSystem::init_data_bitmap_on_format()
{
    std::uint8_t buffer[BLOCK_SIZE];
    std::memset(buffer, 0xFF, BLOCK_SIZE);

    int bytes_in_bitmap = DATA_TABLE_SIZE / BITS_IN_BYTE; // one bit per block

    if (bytes_in_bitmap > 0)
        std::memset(buffer, 0, bytes_in_bitmap);

    int remaining_bits = DATA_TABLE_SIZE % BITS_IN_BYTE;
    if (remaining_bits > 0)
    {
        std::uint8_t mask = 0xFF;
        mask <<= remaining_bits;
        buffer[bytes_in_bitmap] = mask;
    }

    device.write_block(DATA_BITMAP_INDEX, buffer);
}

void FileSystem::init_data_blocks_on_format()
{
    std::uint8_t buffer[BLOCK_SIZE];
    std::memset(buffer, 0, BLOCK_SIZE);

    for (int i = DATA_START_BLOCK; i < TOTAL_BLOCKS_NUMBER; i++)
        device.write_block(i, buffer);
}

FileSystemStatus FileSystem::create_root_dir_inode()
{
    std::uint8_t buffer[BLOCK_SIZE];
    device.read_block(INODE_BITMAP_INDEX, buffer);

    int mask = 1;
    buffer[0] |= mask;
    device.write_block(INODE_BITMAP_INDEX, buffer);

    Inode inode;
    inode = create_inode(0);
    std::memcpy(buffer, &inode, sizeof(Inode));
    device.write_block(INODE_TABLE_START_INDEX, buffer);

    return FileSystemStatus::OK;
}

void FileSystem::save_root_dir_to_device()
{
    std::uint8_t buffer[BLOCK_SIZE];
    int total_entries = static_cast<int>(root_dir_entries.size());

    assert(total_entries * sizeof(DirEntry) + sizeof(int) <= BLOCK_SIZE);

    std::memset(buffer, 0, BLOCK_SIZE);
    std::memcpy(buffer, &total_entries, sizeof(int));

    int offset = sizeof(int);
    for (int i = 0; i < total_entries; i++)
    {
        const DirEntry &entry = root_dir_entries.at(i);
        assert(entry.name[0] != '\0');
        assert(std::memchr(entry.name, '\0', ENTRY_NAME_LENGTH) != nullptr);
        assert(entry.type == 1 || entry.type == 2);
        assert(entry.inode_id >= 0);
        std::memcpy(buffer + offset, &entry, sizeof(DirEntry));
        offset = offset + sizeof(DirEntry);
    }

    device.write_block(DATA_START_BLOCK, buffer);
}

/*
    This method creates an Inode
    type determite if the inode represents a directory or a file
*/
Inode FileSystem::create_inode(int type)
{
    Inode inode;
    inode.type = type;
    inode.size = 0;

    return inode;
}

FileSystemStatus FileSystem::listDir(const std::string &path, std::vector<DirEntry> &entries)
{
    if (!is_formatted)
        return FileSystemStatus::NotFormatted;

    if (path != "/")
        return FileSystemStatus::NotFound;

    entries = root_dir_entries;
    return FileSystemStatus::OK;
}

FileSystemStatus FileSystem::create_entry_in_root(const std::string &entry_name)
{
    if (!is_formatted)
        return FileSystemStatus::NotFormatted;

    // if (!validate_entry_name(entry_name))
    // return FileSystemStatus::UnknownError;

    DirEntry new_entry;

    new_entry.type = 1; // will be change to an enum
    set_entry_name(new_entry, entry_name);

    int new_inode_id = allocate_inode(1);

    if (new_inode_id < 0)
        return FileSystemStatus::UnknownError;
    new_entry.inode_id = new_inode_id;

    Inode new_inode = create_inode(new_entry.type);
    assert(INODE_TABLE_START_INDEX <= device.get_total_blocks_number());
    FileSystemStatus status = write_inode(new_inode_id, new_inode);

    if (status != FileSystemStatus::OK)
        return status;

    root_dir_entries.push_back(new_entry);
    save_root_dir_to_device();

    return FileSystemStatus::OK;
}

FileSystemStatus FileSystem::get_root_entry_inode(const std::string &entry_name, Inode &inode)
{
    if (!is_formatted)
        return FileSystemStatus::NotFormatted;

    size_t len = entry_name.length();

    for (const DirEntry &entry : root_dir_entries)
    {
        if (len == strlen(entry.name) && std::memcmp(entry.name, entry_name.c_str(), len) == 0)
        {
            FileSystemStatus st = read_inode(entry.inode_id, inode);
            return st;
        }
    }

    return FileSystemStatus::NotFound;
}

/********************************** PRIVATE APIs **********************************/

void FileSystem::load_root_dir_from_device()
{
    root_dir_entries.clear();
    std::uint8_t buffer[BLOCK_SIZE];
    device.read_block(ROOT_DIR_BLOCK_INDEX, buffer);

    int total_entries = BLOCK_SIZE / DIR_ENTRY_SIZE;

    DirEntry de;
    for (int i = 0; i < total_entries; i++)
    {
        std::memcpy(&de, buffer + i * DIR_ENTRY_SIZE, DIR_ENTRY_SIZE);

        bool name_valid = (de.name[0] != '\0' && (std::memchr(de.name, '\0', ENTRY_NAME_LENGTH) != nullptr));
        bool type_valid = (de.type == 1 || de.type == 2);
        bool inode_id_valid = de.inode_id >= 0 && de.inode_id < TOTAL_INODE_NUMBER;
        if (name_valid && type_valid && inode_id_valid)
            root_dir_entries.push_back(de);
    }
}

// bool FileSystem::validate_entry_name(const std::string &entry_name)
// {
//     int total_root_entries = root_dir_entries.size();
//     if (total_root_entries >= MAX_ROOT_ENTRIES)
//         return false;

//     int name_len = static_cast<int>(entry_name.length());
//     if (name_len > ENTRY_NAME_LENGTH - 1)
//         return false;

//     if (name_len == 0)
//         return false;

//     for (const DirEntry &e : root_dir_entries)
//         if (strlen(e.name) == entry_name.length() &&
//             std::memcmp(e.name, entry_name.c_str(), entry_name.length()) == 0)
//             return false;

//     return true;
// }

void FileSystem::set_entry_name(DirEntry &entry, const std::string &entry_name)
{
    std::memset(entry.name, 0, ENTRY_NAME_LENGTH);
    std::memcpy(entry.name, entry_name.c_str(), entry_name.length());
}

// This function returns the index of the next free Inode
int FileSystem::allocate_inode(int type)
{
    // get bitmap table
    std::uint8_t buffer[BLOCK_SIZE];
    device.read_block(INODE_BITMAP_INDEX, buffer);

    // scan for the next bit set to off
    int total_bytes_required = (TOTAL_INODE_NUMBER + BITS_IN_BYTE - 1) / BITS_IN_BYTE;
    for (int i = 0; i < total_bytes_required; i++)
    {
        std::uint8_t mask = 1;
        std::uint8_t byte = static_cast<uint8_t>(buffer[i]);

        for (int j = 0; j < BITS_IN_BYTE; j++)
        {
            if ((mask & byte) == 0)
            {
                int inode_id = i * BITS_IN_BYTE + j;
                validate_inode_id(inode_id);
                buffer[i] = static_cast<char>(mask | byte);
                device.write_block(INODE_BITMAP_INDEX, buffer);
                return inode_id;
            }
            mask <<= 1;
        }
    }

    return -1; // no
}

FileSystemStatus FileSystem::write_inode(int inode_id, const Inode &inode)
{
    // making sure the id is legal
    validate_inode_id(inode_id);

    int byte_index = inode_byte_location(inode_id);
    if (!bit_is_on(inode_id, byte_index))
        return FileSystemStatus::NotFound;

    int block_index = get_block_index(inode_id);

    std::uint8_t buffer[BLOCK_SIZE];
    device.read_block(block_index, buffer);

    int offset = (inode_id % INODES_PER_BLOCK) * sizeof(Inode);
    std::memcpy(buffer + offset, &inode, sizeof(Inode));
    device.write_block(block_index, buffer);

    return FileSystemStatus::OK;
}

FileSystemStatus FileSystem::read_inode(int inode_id, Inode &out)
{
    if (!is_formatted)
        return FileSystemStatus::NotFormatted;

    validate_inode_id(inode_id);

    // calculate the wanted bit
    int byte_index = inode_byte_location(inode_id);

    if (!bit_is_on(inode_id, byte_index)) // the inode_id is not in a use
        return FileSystemStatus::NotFound;

    // calculate the data_block_number where the inode lives in
    int block_index = get_block_index(inode_id);

    // reading the wanted block
    std::uint8_t buffer[BLOCK_SIZE];
    device.read_block(block_index, buffer);

    int inode_size = sizeof(Inode);
    int inode_index = inode_id % INODES_PER_BLOCK;
    std::memcpy(&out, buffer + inode_size * inode_index, inode_size);

    return FileSystemStatus::OK;
}

int FileSystem::get_block_index(int inode_id)
{
    int block_index = inode_id / INODES_PER_BLOCK + INODE_TABLE_START_INDEX;
    assert(block_index < device.get_total_blocks_number());

    return block_index;
}

void FileSystem::validate_inode_id(int inode_id)
{
    assert(inode_id >= 0);
    assert(inode_id < TOTAL_INODE_NUMBER);
}

int FileSystem::inode_byte_location(int inode_id)
{
    int byte_index = inode_id / BITS_IN_BYTE;
    assert(byte_index < (TOTAL_INODE_NUMBER + BITS_IN_BYTE - 1) / BITS_IN_BYTE);
    assert(byte_index >= 0);

    return byte_index;
}

bool FileSystem::bit_is_on(int inode_id, const int byte_index)
{
    int bit_index;
    bit_index = inode_id % BITS_IN_BYTE;

    std::uint8_t bitmap_block[BLOCK_SIZE];
    device.read_block(INODE_BITMAP_INDEX, bitmap_block);

    std::uint8_t mask = 1;
    mask <<= bit_index;

    std::uint8_t byte = static_cast<uint8_t>(bitmap_block[byte_index]);

    return (byte & mask) != 0;
}

/*
This method allocate the first free data block in the data table and mark the block as used
free_data_block_number - is the number relative to the data table not the whole device
*/
FileSystemStatus FileSystem::allocate_data_block(int &free_data_block_number)
{
    std::uint8_t buffer[BLOCK_SIZE];
    device.read_block(DATA_BITMAP_INDEX, buffer);

    int free_block_index = -1; // init to -1 to determine if the disk is full

    std::uint8_t mask;
    std::memset(&mask, 0xFF, 1);

    int total_bytes_in_data_bitmap = (DATA_TABLE_SIZE * BITS_IN_BYTE + BITS_IN_BYTE - 1) / BITS_IN_BYTE;
    for (int i = 0; i < total_bytes_in_data_bitmap; i++)
        if (buffer[i] != mask)
        {
            free_block_index = i;
            break;
        }

    if (free_block_index == -1) // the disk is indeed full
        return FileSystemStatus::FullDisk;

    free_data_block_number = free_block_index * BITS_IN_BYTE;

    mask = 1;
    while (mask & buffer[free_block_index])
    {
        mask <<= 1;
        free_data_block_number++;
    }

    buffer[free_block_index] |= mask;
    device.write_block(DATA_BITMAP_INDEX, buffer);

    return FileSystemStatus::OK;
}

/*
data_block_number is the block number in the data table itselt
*/
FileSystemStatus FileSystem::free_data_block(int data_block_number)
{
    if (data_block_number < 0 || data_block_number >= DATA_TABLE_SIZE)
        return FileSystemStatus::OutBoundariesBlock;

    std::uint8_t buffer[BLOCK_SIZE];
    device.read_block(DATA_BITMAP_INDEX, buffer);

    int block_idx = data_block_number / BITS_IN_BYTE; // calc the byte
    int bit_idx = data_block_number % BITS_IN_BYTE;   // calc the bit

    std::uint8_t mask = 1;
    mask <<= bit_idx;

    if (!(buffer[block_idx] & mask)) // the block is already free
        return FileSystemStatus::UnknownError;

    mask = ~mask;
    buffer[block_idx] &= mask; // set to 0 the bit

    device.write_block(DATA_BITMAP_INDEX, buffer);

    return FileSystemStatus::OK;
}
