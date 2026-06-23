#include <gtest/gtest.h>
#include "in_memory_block_device.hpp"
#include "file_system.hpp"
#include "fs_status.hpp"
#include "fs_constants.hpp"
#include <string>
#include <vector>
#include <algorithm>

// ── Fixture ──────────────────────────────────────────────────────────────────

class FileSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        device = std::make_unique<InMemoryBlockDevice>(TOTAL_BLOCKS_NUMBER * BLOCK_SIZE);
        fs = std::make_unique<FileSystem>(*device);
        fs->format();
    }

    std::unique_ptr<InMemoryBlockDevice> device;
    std::unique_ptr<FileSystem> fs;
};

// ── Format / Constructor ──────────────────────────────────────────────────────

TEST_F(FileSystemTest, Format_RootDirectoryIsAccessible)
{
    auto result = fs->list_directory_content(ROOT_INODE_ID, 0);
    EXPECT_TRUE(result.has_value());
}

TEST(FileSystemConstructorTest, Constructor_RecognisesExistingFormat)
{
    InMemoryBlockDevice device(TOTAL_BLOCKS_NUMBER * BLOCK_SIZE);

    {
        FileSystem fs(device);
        fs.format();
        fs.create_file(ROOT_INODE_ID, "persistent.txt");
    }

    FileSystem fs2(device);
    auto result = fs2.lookup(ROOT_INODE_ID, "persistent.txt");
    EXPECT_TRUE(result.has_value());
}

// ── Lookup ────────────────────────────────────────────────────────────────────

TEST_F(FileSystemTest, Lookup_DotEntry_Found)
{
    auto result = fs->lookup(ROOT_INODE_ID, ".");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().inode_id, ROOT_INODE_ID);
}

TEST_F(FileSystemTest, Lookup_DotDotEntry_PointsToRoot)
{
    auto result = fs->lookup(ROOT_INODE_ID, "..");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().inode_id, ROOT_INODE_ID);
}

TEST_F(FileSystemTest, Lookup_NonExistentEntry_ReturnsError)
{
    auto result = fs->lookup(ROOT_INODE_ID, "nonexistent");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FileSystemStatus::EntryNotFound);
}

// ── Create File ───────────────────────────────────────────────────────────────

TEST_F(FileSystemTest, CreateFile_ReturnsValidInodeId)
{
    auto result = fs->create_file(ROOT_INODE_ID, "test.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_GE(result.value(), 1); // 0 is root
}

TEST_F(FileSystemTest, CreateFile_NameTooLong_ReturnsError)
{
    std::string long_name(ENTRY_NAME_LENGTH + 1, 'a');
    auto result = fs->create_file(ROOT_INODE_ID, long_name);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FileSystemStatus::EntryNameTooLong);
}

TEST_F(FileSystemTest, CreateFile_ThenLookup_Found)
{
    auto create_result = fs->create_file(ROOT_INODE_ID, "hello.txt");
    ASSERT_TRUE(create_result.has_value());

    auto lookup_result = fs->lookup(ROOT_INODE_ID, "hello.txt");
    ASSERT_TRUE(lookup_result.has_value());
    EXPECT_EQ(lookup_result.value().inode_id, create_result.value());
    EXPECT_EQ(lookup_result.value().type, EntryType::File);
}

TEST_F(FileSystemTest, CreateMultipleFiles_AllLookupSucceed)
{
    std::vector<int> inode_ids;
    for (int i = 0; i < 5; i++)
    {
        auto result = fs->create_file(ROOT_INODE_ID, "file" + std::to_string(i));
        ASSERT_TRUE(result.has_value());
        inode_ids.push_back(result.value());
    }

    for (int i = 0; i < 5; i++)
    {
        auto result = fs->lookup(ROOT_INODE_ID, "file" + std::to_string(i));
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value().inode_id, inode_ids[i]);
    }
}

// ── Write / Read File ─────────────────────────────────────────────────────────

TEST_F(FileSystemTest, WriteFile_ThenReadBack_DataMatches)
{
    auto create_result = fs->create_file(ROOT_INODE_ID, "data.bin");
    ASSERT_TRUE(create_result.has_value());
    int inode_id = create_result.value();

    std::string message = "Hello, FileSystem!";
    std::vector<uint8_t> write_data(message.begin(), message.end());

    auto write_result = fs->write_file(inode_id, write_data, 0);
    ASSERT_TRUE(write_result.has_value());
    EXPECT_EQ(write_result.value(), write_data.size());

    std::vector<uint8_t> read_data(write_data.size());
    auto read_result = fs->read_file(inode_id, read_data, 0);
    ASSERT_TRUE(read_result.has_value());
    EXPECT_EQ(read_result.value(), write_data.size());
    EXPECT_EQ(read_data, write_data);
}

TEST_F(FileSystemTest, WriteFile_WithOffset_OverwritesCorrectBytes)
{
    auto create_result = fs->create_file(ROOT_INODE_ID, "offset.bin");
    ASSERT_TRUE(create_result.has_value());
    int inode_id = create_result.value();

    std::vector<uint8_t> initial = {'A', 'A', 'B', 'B', 'C', 'C'};
    fs->write_file(inode_id, initial, 0);

    std::vector<uint8_t> patch = {'X', 'X'};
    fs->write_file(inode_id, patch, 2);

    std::vector<uint8_t> result(6);
    fs->read_file(inode_id, result, 0);

    std::vector<uint8_t> expected = {'A', 'A', 'X', 'X', 'C', 'C'};
    EXPECT_EQ(result, expected);
}

TEST_F(FileSystemTest, WriteFile_AcrossBlockBoundary_DataMatches)
{
    auto create_result = fs->create_file(ROOT_INODE_ID, "big.bin");
    ASSERT_TRUE(create_result.has_value());
    int inode_id = create_result.value();

    // Write more than one block
    std::vector<uint8_t> big_data(BLOCK_SIZE + 512, 0xAB);
    auto write_result = fs->write_file(inode_id, big_data, 0);
    ASSERT_TRUE(write_result.has_value());

    std::vector<uint8_t> read_data(big_data.size());
    auto read_result = fs->read_file(inode_id, read_data, 0);
    ASSERT_TRUE(read_result.has_value());
    EXPECT_EQ(read_data, big_data);
}

// ── Delete File ───────────────────────────────────────────────────────────────

TEST_F(FileSystemTest, DeleteFile_Success_LookupFails)
{
    auto create_result = fs->create_file(ROOT_INODE_ID, "todelete.txt");
    ASSERT_TRUE(create_result.has_value());
    int inode_id = create_result.value();

    FileSystemStatus status = fs->delete_entry(ROOT_INODE_ID, inode_id);
    EXPECT_EQ(status, FileSystemStatus::OK);

    auto lookup_result = fs->lookup(ROOT_INODE_ID, "todelete.txt");
    EXPECT_FALSE(lookup_result.has_value());
}

TEST_F(FileSystemTest, DeleteFile_ThenRecreate_Success)
{
    auto c1 = fs->create_file(ROOT_INODE_ID, "reuse.txt");
    ASSERT_TRUE(c1.has_value());
    fs->delete_entry(ROOT_INODE_ID, c1.value());

    auto c2 = fs->create_file(ROOT_INODE_ID, "reuse.txt");
    EXPECT_TRUE(c2.has_value());
}

// ── Get Attributes ────────────────────────────────────────────────────────────

TEST_F(FileSystemTest, GetAttributes_RootDirectory_CorrectType)
{
    auto result = fs->get_attributes(ROOT_INODE_ID);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().type, EntryType::Directory);
}

TEST_F(FileSystemTest, GetAttributes_RootDirectory_TwoEntries)
{
    auto result = fs->get_attributes(ROOT_INODE_ID);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size, 2u); // "." and ".."
}

TEST_F(FileSystemTest, GetAttributes_NewFile_ZeroSize)
{
    auto create_result = fs->create_file(ROOT_INODE_ID, "empty.txt");
    ASSERT_TRUE(create_result.has_value());

    auto result = fs->get_attributes(create_result.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().type, EntryType::File);
    EXPECT_EQ(result.value().size, 0u);
}

TEST_F(FileSystemTest, GetAttributes_FileAfterWrite_SizeUpdated)
{
    auto create_result = fs->create_file(ROOT_INODE_ID, "sized.txt");
    ASSERT_TRUE(create_result.has_value());
    int inode_id = create_result.value();

    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    fs->write_file(inode_id, data, 0);

    auto result = fs->get_attributes(inode_id);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size, 5u);
}

TEST_F(FileSystemTest, GetAttributes_InvalidInode_ReturnsError)
{
    auto result = fs->get_attributes(-1);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FileSystemStatus::OutOfBounds);
}

// ── Create Directory ──────────────────────────────────────────────────────────

TEST_F(FileSystemTest, CreateDirectory_ReturnsValidInodeId)
{
    auto result = fs->create_directory(ROOT_INODE_ID, "mydir");
    ASSERT_TRUE(result.has_value());
    EXPECT_GE(result.value(), 1);
}

TEST_F(FileSystemTest, CreateDirectory_ThenLookup_CorrectType)
{
    auto create_result = fs->create_directory(ROOT_INODE_ID, "docs");
    ASSERT_TRUE(create_result.has_value());

    auto lookup_result = fs->lookup(ROOT_INODE_ID, "docs");
    ASSERT_TRUE(lookup_result.has_value());
    EXPECT_EQ(lookup_result.value().type, EntryType::Directory);
}

TEST_F(FileSystemTest, CreateDirectory_HasDotAndDotDotEntries)
{
    auto create_result = fs->create_directory(ROOT_INODE_ID, "subdir");
    ASSERT_TRUE(create_result.has_value());
    int dir_inode_id = create_result.value();

    auto dot_result = fs->lookup(dir_inode_id, ".");
    ASSERT_TRUE(dot_result.has_value());
    EXPECT_EQ(dot_result.value().inode_id, dir_inode_id);

    auto dotdot_result = fs->lookup(dir_inode_id, "..");
    ASSERT_TRUE(dotdot_result.has_value());
    EXPECT_EQ(dotdot_result.value().inode_id, ROOT_INODE_ID);
}

// ── Delete Directory ──────────────────────────────────────────────────────────

TEST_F(FileSystemTest, DeleteDirectory_Empty_Success)
{
    auto create_result = fs->create_directory(ROOT_INODE_ID, "emptydir");
    ASSERT_TRUE(create_result.has_value());
    int dir_inode_id = create_result.value();

    FileSystemStatus status = fs->delete_entry(ROOT_INODE_ID, dir_inode_id);
    EXPECT_EQ(status, FileSystemStatus::OK);

    auto lookup_result = fs->lookup(ROOT_INODE_ID, "emptydir");
    EXPECT_FALSE(lookup_result.has_value());
}

TEST_F(FileSystemTest, DeleteDirectory_NonEmpty_ReturnsError)
{
    auto dir_result = fs->create_directory(ROOT_INODE_ID, "fulldir");
    ASSERT_TRUE(dir_result.has_value());
    int dir_inode_id = dir_result.value();

    auto file_result = fs->create_file(dir_inode_id, "file.txt");
    ASSERT_TRUE(file_result.has_value());

    FileSystemStatus status = fs->delete_entry(ROOT_INODE_ID, dir_inode_id);
    EXPECT_EQ(status, FileSystemStatus::InodeNotEmpty);
}

// ── List Directory Content ────────────────────────────────────────────────────

TEST_F(FileSystemTest, ListDirectory_Root_ContainsDotAndDotDot)
{
    auto entries_result = fs->list_directory_content(ROOT_INODE_ID, 0);
    ASSERT_TRUE(entries_result.has_value());

    auto& entries = entries_result.value();
    bool has_dot = std::any_of(entries.begin(), entries.end(),
        [](const Entry& e) { return e.inode_id != -1 && std::string(e.name) == "."; });
    bool has_dotdot = std::any_of(entries.begin(), entries.end(),
        [](const Entry& e) { return e.inode_id != -1 && std::string(e.name) == ".."; });

    EXPECT_TRUE(has_dot);
    EXPECT_TRUE(has_dotdot);
}

TEST_F(FileSystemTest, ListDirectory_AfterCreateFile_ShowsFile)
{
    fs->create_file(ROOT_INODE_ID, "visible.txt");

    auto entries_result = fs->list_directory_content(ROOT_INODE_ID, 0);
    ASSERT_TRUE(entries_result.has_value());

    auto& entries = entries_result.value();
    bool found = std::any_of(entries.begin(), entries.end(),
        [](const Entry& e) { return e.inode_id != -1 && std::string(e.name) == "visible.txt"; });

    EXPECT_TRUE(found);
}

TEST_F(FileSystemTest, ListDirectory_InvalidInode_ReturnsError)
{
    auto result = fs->list_directory_content(-1, 0);
    EXPECT_FALSE(result.has_value());
}

// ── Private Function Tests ────────────────────────────────────────────────────

class FileSystemInternalTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        device = std::make_unique<InMemoryBlockDevice>(TOTAL_BLOCKS_NUMBER * BLOCK_SIZE);
        fs = std::make_unique<FileSystem>(*device);
        fs->format();
    }

    std::expected<Inode, FileSystemStatus> call_get_inode(int inode_id)
    {
        return fs->get_inode(inode_id);
    }

    FileSystemStatus call_write_inode(int inode_id, const Inode &inode)
    {
        return fs->write_inode(inode_id, inode);
    }

    std::expected<int, FileSystemStatus> call_allocate_inode()
    {
        return fs->allocate_inode();
    }

    FileSystemStatus call_free_inode(int inode_id)
    {
        return fs->free_inode(inode_id);
    }

    std::expected<int, FileSystemStatus> call_allocate_data_block()
    {
        return fs->allocate_data_block();
    }

    FileSystemStatus call_free_data_block(int block_number)
    {
        return fs->free_data_block(block_number);
    }

    std::expected<int, FileSystemStatus> call_get_block_index(Inode &inode, int target_block)
    {
        return fs->get_block_index(inode, target_block);
    }

    std::expected<int, FileSystemStatus> call_get_inode_by_path(std::string_view path)
    {
        return fs->get_inode_by_path(path);
    }

    std::unique_ptr<InMemoryBlockDevice> device;
    std::unique_ptr<FileSystem> fs;
};

// ── write_inode / get_inode ───────────────────────────────────────────────────

TEST_F(FileSystemInternalTest, WriteInode_ThenGetInode_DataMatches)
{
    // inode 1 is free after format (root uses 0)
    auto alloc_res = call_allocate_inode();
    ASSERT_TRUE(alloc_res.has_value());
    int inode_id = alloc_res.value();

    Inode inode{};
    inode.type = EntryType::File;
    inode.size = 42;
    inode.link_count = 1;
    std::fill(inode.direct_blocks, inode.direct_blocks + TOTAL_DIRECT_BLOCKS, -1);

    FileSystemStatus status = call_write_inode(inode_id, inode);
    ASSERT_EQ(status, FileSystemStatus::OK);

    auto read_res = call_get_inode(inode_id);
    ASSERT_TRUE(read_res.has_value());
    EXPECT_EQ(read_res.value().type, EntryType::File);
    EXPECT_EQ(read_res.value().size, 42);
    EXPECT_EQ(read_res.value().link_count, 1);
}

TEST_F(FileSystemInternalTest, GetInode_OutOfBounds_ReturnsError)
{
    auto result = call_get_inode(-1);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FileSystemStatus::OutOfBounds);

    result = call_get_inode(TOTAL_INODE_NUMBER);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FileSystemStatus::OutOfBounds);
}

TEST_F(FileSystemInternalTest, GetInode_FreeInode_ReturnsNotFound)
{
    // inode 1 is free right after format
    auto result = call_get_inode(1);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FileSystemStatus::InodeNotFound);
}

// ── allocate_inode / free_inode ───────────────────────────────────────────────

TEST_F(FileSystemInternalTest, AllocateInode_ReturnsSequentialIds)
{
    for (int expected_id = 1; expected_id <= 5; expected_id++) // 0 is root
    {
        auto result = call_allocate_inode();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), expected_id);
    }
}

TEST_F(FileSystemInternalTest, FreeInode_ThenReallocate_ReusesSameId)
{
    auto alloc1 = call_allocate_inode();
    ASSERT_TRUE(alloc1.has_value());
    int inode_id = alloc1.value();

    FileSystemStatus status = call_free_inode(inode_id);
    ASSERT_EQ(status, FileSystemStatus::OK);

    auto alloc2 = call_allocate_inode();
    ASSERT_TRUE(alloc2.has_value());
    EXPECT_EQ(alloc2.value(), inode_id); // same slot is reused
}

TEST_F(FileSystemInternalTest, AllocateInode_WhenFull_ReturnsError)
{
    for (int i = 1; i < TOTAL_INODE_NUMBER; i++) // 0 already used by root
        call_allocate_inode();

    auto result = call_allocate_inode();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FileSystemStatus::FullInode);
}

// ── allocate_data_block / free_data_block ─────────────────────────────────────

TEST_F(FileSystemInternalTest, AllocateDataBlock_ReturnsAbsoluteBlockIndex)
{
    auto result = call_allocate_data_block();
    ASSERT_TRUE(result.has_value());
    EXPECT_GE(result.value(), DATA_START_BLOCK);
}

TEST_F(FileSystemInternalTest, FreeDataBlock_ThenReallocate_ReusesSameBlock)
{
    auto alloc1 = call_allocate_data_block();
    ASSERT_TRUE(alloc1.has_value());
    int block_index = alloc1.value();

    // free_data_block takes the data-table-relative index
    int relative = block_index - DATA_START_BLOCK;
    FileSystemStatus status = call_free_data_block(relative);
    ASSERT_EQ(status, FileSystemStatus::OK);

    auto alloc2 = call_allocate_data_block();
    ASSERT_TRUE(alloc2.has_value());
    EXPECT_EQ(alloc2.value(), block_index);
}

TEST_F(FileSystemInternalTest, AllocateDataBlock_WhenFull_ReturnsError)
{
    for (int i = 0; i < DATA_TABLE_SIZE; i++)
        call_allocate_data_block();

    auto result = call_allocate_data_block();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FileSystemStatus::FullDisk);
}

// ── get_block_index ───────────────────────────────────────────────────────────

TEST_F(FileSystemInternalTest, GetBlockIndex_NoBlock_ReturnsError)
{
    Inode inode{};
    std::fill(inode.direct_blocks, inode.direct_blocks + TOTAL_DIRECT_BLOCKS, -1);

    auto result = call_get_block_index(inode, 0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FileSystemStatus::OutOfBounds);
}

TEST_F(FileSystemInternalTest, GetBlockIndex_OutOfRange_ReturnsError)
{
    Inode inode{};
    std::fill(inode.direct_blocks, inode.direct_blocks + TOTAL_DIRECT_BLOCKS, 10);

    auto result = call_get_block_index(inode, TOTAL_DIRECT_BLOCKS);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FileSystemStatus::OutOfBounds);
}

TEST_F(FileSystemInternalTest, GetBlockIndex_ValidBlock_ReturnsIndex)
{
    Inode inode{};
    std::fill(inode.direct_blocks, inode.direct_blocks + TOTAL_DIRECT_BLOCKS, -1);
    inode.direct_blocks[0] = DATA_START_BLOCK;

    auto result = call_get_block_index(inode, 0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), DATA_START_BLOCK);
}

// ── get_inode_by_path ─────────────────────────────────────────────────────────

TEST_F(FileSystemInternalTest, GetInodeByPath_Root_ReturnsRootId)
{
    auto result = call_get_inode_by_path("/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), ROOT_INODE_ID);
}

TEST_F(FileSystemInternalTest, GetInodeByPath_SingleLevel_ReturnsFileId)
{
    auto create_res = fs->create_file(ROOT_INODE_ID, "readme.txt");
    ASSERT_TRUE(create_res.has_value());

    auto result = call_get_inode_by_path("/readme.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), create_res.value());
}

TEST_F(FileSystemInternalTest, GetInodeByPath_TwoLevels_ReturnsFileId)
{
    auto dir_res = fs->create_directory(ROOT_INODE_ID, "docs");
    ASSERT_TRUE(dir_res.has_value());

    auto file_res = fs->create_file(dir_res.value(), "notes.txt");
    ASSERT_TRUE(file_res.has_value());

    auto result = call_get_inode_by_path("/docs/notes.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), file_res.value());
}

TEST_F(FileSystemInternalTest, GetInodeByPath_NonExistent_ReturnsError)
{
    auto result = call_get_inode_by_path("/no_such_file.txt");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FileSystemStatus::EntryNotFound);
}

// ── expand_directory (indirect) ───────────────────────────────────────────────

TEST_F(FileSystemInternalTest, ExpandDirectory_FillBeyondOneBlock_AllEntriesAccessible)
{
    // Root starts with 2 entries (. and ..). One block holds ENTRIES_PER_BLOCK entries.
    // Adding ENTRIES_PER_BLOCK - 1 files forces expand_directory to allocate a second block.
    const int files_to_create = ENTRIES_PER_BLOCK - 1;

    std::vector<int> inode_ids;
    for (int i = 0; i < files_to_create; i++)
    {
        auto result = fs->create_file(ROOT_INODE_ID, "f" + std::to_string(i));
        ASSERT_TRUE(result.has_value()) << "failed at file " << i;
        inode_ids.push_back(result.value());
    }

    for (int i = 0; i < files_to_create; i++)
    {
        auto result = fs->lookup(ROOT_INODE_ID, "f" + std::to_string(i));
        ASSERT_TRUE(result.has_value()) << "lookup failed for file " << i;
        EXPECT_EQ(result.value().inode_id, inode_ids[i]);
    }
}
