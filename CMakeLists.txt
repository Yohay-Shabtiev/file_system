cmake_minimum_required(VERSION 3.14)
project(FilesystemProject CXX)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Core library ----
add_library(filesystem_core
    src/in_memory_block_device.cpp
    src/file_system.cpp
)

target_include_directories(filesystem_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
)

# ---- Main executable ----
add_executable(FilesystemProject
    main.cpp
)

target_link_libraries(FilesystemProject
    PRIVATE
        filesystem_core
)

# ---- Tests (optional) ----
option(BUILD_TESTS "Build tests" OFF)

if(BUILD_TESTS)
    include(FetchContent)

    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    )

    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(fs_tests
        tests/test_file_system_stage1.cpp
        tests/test_file_system_stage2.cpp
        tests/test_file_system_data_manager.cpp
    )

    target_link_libraries(fs_tests
        PRIVATE
            filesystem_core
            gtest_main
    )

    add_test(NAME fs_tests COMMAND fs_tests)
endif()