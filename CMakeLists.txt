cmake_minimum_required(VERSION 3.16)
project(MyFS CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Include paths ─────────────────────────────────────────────────
include_directories(
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/rpc
)

# ── Common FS sources ─────────────────────────────────────────────
set(FS_SOURCES
    src/file_system.cpp
    src/in_memory_block_device.cpp
)

# ── RPC Server ────────────────────────────────────────────────────
add_executable(server
    rpc/server.cpp
    ${FS_SOURCES}
)
target_include_directories(server PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/rpc
    ${CMAKE_SOURCE_DIR}/rpc/includes
)

# ── FUSE Client ───────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

add_executable(fuse_client
    fuse_client.cpp
)
target_include_directories(fuse_client PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/rpc
    ${CMAKE_SOURCE_DIR}/rpc/includes
    ${FUSE3_INCLUDE_DIRS}
)
target_compile_options(fuse_client PRIVATE ${FUSE3_CFLAGS_OTHER})
target_link_libraries(fuse_client PRIVATE ${FUSE3_LIBRARIES})