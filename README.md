File System & Demo NFS RPC Layer
An implementation of an in-memory filesystem with a demo NFS RPC layer over TCP, written in modern C++.
Built to understand how remote storage systems work under the hood — from raw block devices up to network-accessible file operations.

About the File System
The filesystem implementation is based on Inode and Entry structs. An entry can be a directory or a file, and an Inode holds the metadata of its corresponding entry.

Constraints:
The filesystem supports up to 128 entries.
Each file is limited to 48 KB.


About the RPC (Remote Procedure Calls) Layer

The filesystem functionality (the server) is invoked from a remote machine (the client) over a TCP socket.
The server is stateless, so the client manages the session state within each request (send & recv).
For example, if a client wants to write a multiple-block file — the client is the one chunking the data and keeping the server stateless.
Multi-threading is supported on the networking side. The server locks the filesystem when a thread invokes a filesystem API, so parallelism is primarily on the networking side.
Layer separation is supported — the RPC layer translates any filesystem structure to a corresponding client struct.

Supported operations:
1. GETATTR - Get the attributes of an entry
2. LOOKUP - Look for an entry by name in a directory
3. CREATE_FILE - Create a new file
4. MKDIR - Create a new directory
5. READ - Read from a file
6. WRITE - Write to a file
7. READDIR - List directory contents
8. DELET - EDelete an entry

Build
bash# server
g++-13 -std=c++23 rpc/server.cpp src/file_system.cpp src/in_memory_block_device.cpp \
    -I includes -I rpc/includes -lpthread -o server

bash# client
g++-13 -std=c++23 rpc/client.cpp -I includes -I rpc/includes -o client

Run
Use a split terminal:
bash# terminal 1 — run the server
./server

bash# terminal 2 — run a single client
./client

bash# terminal 2 — run multiple clients concurrently
./client & ./client & ./client & ./client & ./client

What I Learned
Linux-based filesystem fundamentals — superblocks, bitmaps, inode tables.
NFSv3 vs NFSv4 differences.
How TCP streams work.
Stateless vs stateful servers — pros and cons.
Linux file descriptors concept.
General vs fine-grained locking in concurrent storage systems.
