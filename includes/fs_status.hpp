#pragma once

enum class FileSystemStatus
{
    OK,
    NotFormatted,
    NotFound,
    NotDirectory,
    FullDisk,
    FullDirectory,
    FullInode,
    OutOfBounds,
    UnknownError,
    EntryNameTooLong,
    EntryTypeError,
    EntryNotFound
};
