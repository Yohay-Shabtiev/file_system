#pragma once

enum class FileSystemStatus
{
    OK,
    NotFormatted,
    NotFound,
    NotDirectory,
    FullDisk,
    FullInode,
    OutOfBounds,
    UnknownError
};