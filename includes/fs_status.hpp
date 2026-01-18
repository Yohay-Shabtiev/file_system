#pragma once

enum class FileSystemStatus
{
    OK,
    NotFormatted,
    NotFound,
    NotDirectory,
    FullDisk,
    OutBoundariesBlock,
    UnknownError
};