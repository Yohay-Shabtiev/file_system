The File System is designed for NFS (Network File System) protocol integration. 
The main goals of this project are to understand the basic fundamentals of a file system and the basic fundamentals of a network protocol. 

The file system is built using an "in-memory" approach, where the data is available until the session is terminated.


The file system management is handled by 2 managers - 'Data Manager' and 'Inode Manager'.

The 'Data Manager' purpose is to handle the resource management by maintaining a bitmap for the data blocks in the device we able to allocate data blocks for the 'Entry' elements. 

The file system stores 'Entry' elements, specifically 'Directory' or 'File'.
Each entry is represented by an 'Inode' struct, which represents the metadata of each entry.

The 'Inode Manager' purpose is to handle the management of the Entries in the file system
by maintaining a bit map for the inodes in the file system