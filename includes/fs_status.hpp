#pragma once

enum class FileSystemStatus
{
    OK,            // 0
    NotFormatted,  // 1
    NotFound,      // 2
    NotDirectory,  // 3
    FullDisk,      // 4
    FullDirectory, // 5
    FullInode,     // 6
    OutOfBounds,   // 7
    UnknownError,
    EntryNameTooLong,
    EntryTypeError,
    EntryNotFound,
    NotEmpty,
    InodeNotFound,
    InodeNotEmpty
};
