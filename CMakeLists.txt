cmake_minimum_required(VERSION 3.14)
project(FilesystemProject CXX)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Core library with your filesystem code ----
add_library(filesystem_core
    src/in_memory_block_device.cpp
    src/file_system.cpp
)

target_include_directories(filesystem_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
)

# ---- GoogleTest via FetchContent ----
include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)

# Download + add gtest / gtest_main targets
FetchContent_MakeAvailable(googletest)

# ---- Tests ----
enable_testing()  # tells CMake that we will have tests

add_executable(fs_tests
    tests/test_in_memory_block_device.cpp
    tests/test_file_system.cpp
)

target_link_libraries(fs_tests
    PRIVATE
        filesystem_core   # your library with FS code
        gtest_main        # GoogleTest main()
)

add_test(
    NAME fs_tests
    COMMAND fs_tests
)
