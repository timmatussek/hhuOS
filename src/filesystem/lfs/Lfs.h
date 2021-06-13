/*
 * Copyright (C) 2021 Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Michael Schoettner, Tim Matussek
 * Heinrich-Heine University
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#ifndef __Lfs_include__
#define __Lfs_include__

#include "filesystem/core/FsDriver.h"
#include "lib/util/SmartPointer.h"
#include "Superblock.h"
#include "InodeMapEntry.h"
#include "Inode.h"

// forward declare
class LfsFlushCallback;

/**
 * Magic number to identify valid lfs.
 */
#define LFS_MAGIC 0x2153464c

/**
 * Smallest addressable unit of lfs.
 */
#define BLOCK_SIZE 4096

/**
 * Size of a segment. Must be a multiple of BLOCK_SIZE.
 */
#define SEGMENT_SIZE (BLOCK_SIZE * 256)

/**
 * How many blocks are in a segment.
 */
#define BLOCKS_PER_SEGMENT (SEGMENT_SIZE / BLOCK_SIZE)

/**
 * How many block addresses are stored in a indirect block.
 */
#define BLOCKS_PER_INDIRECT_BLOCK (BLOCK_SIZE / sizeof(uint64_t))

/**
 * How many block addresses are stored in a indirect block.
 */
#define BLOCKS_PER_DOUBLY_INDIRECT_BLOCK (BLOCKS_PER_INDIRECT_BLOCK * BLOCKS_PER_INDIRECT_BLOCK)

class Lfs {
private:
    /**
     * The device this filesystem is read from or written to.
     */
    StorageDevice &device;

    /**
     * True if there are changes in memory that need to be flushed
     * to disk.
     */
    bool dirty;

    /**
     * A thread that periodically calls flush.
     */
    Util::SmartPointer<LfsFlushCallback> flushCallback;

    /**
     * Keeps track of the next unused inode number.
     */
    uint64_t nextInodeNumber;

    /**
     * Superblock of current lfs.
     */
    Superblock superblock;

    /**
     * Device Sectors per lfs block.
     */
    size_t sectorsPerBlock;

    /**
     * In-memory cache of inode map.
     * 
     * Maps an inode number to its inode map entry
     */
    Util::HashMap<uint64_t, InodeMapEntry> inodeMap;

    /**
     * In-memory cache of inodes.
     * 
     * Maps an inode number to its inode.
     */
    Util::HashMap<uint64_t, Inode> inodeCache;
    
    /**
     * Buffer for various block operations.
     */
    Util::Array<uint8_t> blockBuffer = Util::Array<uint8_t>(BLOCK_SIZE);

    /**
     * Buffer for various segment operations.
     */
    Util::Array<uint8_t> segmentBuffer = Util::Array<uint8_t>(SEGMENT_SIZE);


    /**
     * Next empty block in segment buffer.
     */
    size_t nextBlockInSegment = 0;

    /**
     * Get the file or directory name of a path.
     * 
     * @param path A path to a file or directory.
     * 
     * @return The last element of the path.
     */
    String getFileName(const String &path);

    /**
     * Find the inode number of a path.
     * 
     * @param path A path to a file or directory.
     * 
     * @return Inode number of path. 0 if not found.
     */
    uint64_t getInodeNumber(const String &path);

    /**
     * Find the inode number of the parent of a path.
     * 
     * @param path A path to a file or directory.
     * 
     * @return Inode number of parent. 0 if not found.
     */
    uint64_t getParentInodeNumber(const String &path);

    /**
     * Find the inode of a inode number.
     * 
     * @param inodeNumber A inode number.
     * 
     * @return Inode of inodeNumber.
     */
    Inode getInode(uint64_t inodeNumber);

    /**
     * Rounds up an address to the nearest multiple of
     * BLOCK_SIZE, if not already a multiple of BLOCK_SIZE.
     * 
     * @return rounded address
     */
    uint64_t roundUpBlockAddress(uint64_t addr);

    /**
     * Rounds down an address to the nearest multiple of
     * BLOCK_SIZE, if not already a multiple of BLOCK_SIZE.
     * 
     * @return rounded address
     */
    uint64_t roundDownBlockAddress(uint64_t addr);

    /**
     * 
     * Read a block from a file into buffer.
     * 
     * @param inode inode of the file
     * @param blockNumberInFile relative block number in file
     * @param buffer buffer where result block is stored
     * 
     */
    void getBlockInFile(const Inode &inode, uint64_t blockNumberInFile, uint8_t* buffer);

    /**
     * 
     * Write a block to a file. Will allocate a new block and 
     * write data from buffer to it.
     * 
     * @param inode inode of the file
     * @param blockNumberInFile relative block number in file
     * @param buffer buffer with data
     * 
     */
    void setBlockInFile(Inode &inode, uint64_t blockNumberInFile, const uint8_t* buffer);

    /**
     * 
     * Add a new directory entry to a directory.
     * Writes new blocks to cache as necessary.
     * 
     * @param dirInodeNumber inode number of directory
     * @param name name of new entry
     * @param entryInodeNumber inode number of new entry
     * 
     */
    void addDirectoryEntry(uint64_t dirInodeNumber, const String &name, uint64_t entryInodeNumber);

    /**
     * 
     * Delete a directory entry from a directory.
     * Writes new blocks to cache as necessary.
     * 
     * @param dirInodeNumber inode number of directory
     * @param name name of entry to delete
     * 
     */
    void deleteDirectoryEntry(uint64_t dirInodeNumber, const String &name);

    /**
     * 
     * Find a directory entry from a directory.
     * 
     * @param dirInodeNumber inode number of directory
     * @param name name of entry to find
     * 
     * @return inode number of entry if found, 0 otherwise
     * 
     */
    uint64_t findDirectoryEntry(uint64_t dirInodeNumber, const String &name);

    /**
     * 
     * Get an array of names of all directory entries in a directory.
     * 
     * @param dirInodeNumber inode number of directory
     * 
     * @return array of strings with names of entries
     * 
     */
    Util::Array<String> readDirectoryEntries(uint64_t dirInodeNumber);

    /**
     * 
     * Write a block into segment buffer at next free location.
     * 
     * @param block data to be written
     * 
     */
    void writeBlockToSegmentBuffer(const uint8_t* block);

    /**
     * 
     * Write segment buffer to disk.
     * 
     */
    void flushSegmentBuffer();

public:
    /**
     * Constructor.
     * 
     * Initialize an empty lfs.
     * 
     * @param mount If true the lfs is read from device, otherwise it is a fresh empty one
     */
    Lfs(StorageDevice &device, bool mount = false);

    /**
     * Destructor.
     */
    ~Lfs();

    /**
     * Forces all caches to be written to disk.
     */
    void flush();

    /**
     * Read bytes from the files data.
     * If (pos + numBytes) is greater than the datas length, END_OF_FILE shall be appended.
     * If pos is greater than the datas length, END_OF_FILE shall be the first and only character,
     * that is written to buf.
     *
     * @param path The path to a file
     * @param buf The buffer to write to (Needs to be allocated already!)
     * @param pos The offset
     * @param numBytes The amount of bytes to read
     * 
     * @return The amount of actually read bytes
     */
    uint64_t readData(const String &path, char *buf, uint64_t pos, uint64_t numBytes);

    /**
     * Write bytes to the files data. If the offset points right into the existing data,
     * it shall be overwritten with the new data. If the new data does not fit, the data size shall be increased.
     * 
     * @param path The path to a file
     * @param buf The data to write
     * @param pos The offset
     * @param numBytes The amount of bytes to write
     * 
     * @return The amount of actually written bytes
     */
    uint64_t writeData(const String &path, char *buf, uint64_t pos, uint64_t length);

    /**
     * Create a new empty file or directory at a given path.
     * The parent-directory of the new file must exist beforehand.
     * 
     * @param path The path to a file
     * @param fileType The filetype
     * 
     * @return true on success.
     */
    bool createNode(const String &path, uint8_t fileType);

    /**
     * Delete an existing file or directory at a given path.
     * The file must be a regular file or an empty folder (a leaf in the filesystem tree).
     * 
     * @param path The path to a file
     * 
     * @return true on success.
     */
    bool deleteNode(const String &path);

    /**
     * Returns the filetype.
     * 
     * @param path The path to a file
     */
    uint8_t getFileType(const String &path);

    /**
     * Get the length (in bytes) of the files data.
     * If the node is a directory, this should always return 0.
     * 
     * @param path The path to a file
     */
    uint64_t getLength(const String &path);

    /**
     * Get the files children.
     * 
     * @param path The path to a file
     */
    Util::Array<String> getChildren(const String &path);
};

#endif