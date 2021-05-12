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

/**
 * Magic number to identify valid lfs.
 */
#define LFS_MAGIC 0x4c465321

/**
 * Smallest addressable unit of lfs.
 */
#define BLOCK_SIZE 4096

/**
 * How many block addresses are stored in a indirect block.
 */
#define BLOCKS_PER_INDIRECT_BLOCK (BLOCK_SIZE / sizeof(uint64_t))

/**
 * The superblock contains information about the filesystem.
 * It is always at block 0.
 */
struct Superblock
{
    /**
     * Magic number to identify valid lfs.
     */
    uint32_t magic;

    /**
     * The starting block of the current inode map.
     */
    uint64_t inodeMapPosition;

    /**
     * The size in blocks of the current inode map.
     */
    uint64_t inodeMapSize;

    /**
     * The segment number of the next empty segment.
     */
    uint64_t currentSegment;
};

/**
 * The inode map contains the position of each active inode.
 */
struct InodeMapEntry
{
    /**
     * The block the inode is stored in.
     */
    uint64_t inodePosition;

    /**
     * The offset inside the block.
     */
    uint32_t inodeOffset;
};

/**
 * An inode contains metadata and data blocks of a file or directory.
 */
struct Inode
{
    /**
     * True if the in-memory inode changed and needs to be written to disk.
     */
    bool dirty;

    /**
     * Size in bytes of the file. Always 0 for directories.
     */
    uint64_t size;

    /**
     * Filetype of file.
     */
    uint8_t fileType;

    /**
     * Pointers to data blocks.
     */
    uint64_t directBlocks[10];

    /**
     * Pointer to a block containing pointers to data blocks.
     */
    uint64_t indirectBlocks;

    /**
     * Pointer to a block containing pointers to blocks containing pointers to data blocks.
     */
    uint64_t doublyIndirectBlocks;
};

/**
 * A data block contains actual data of a file.
 */
struct DataBlock
{
    /**
     * True if the in-memory datablock changed and needs to be written to disk.
     */
    bool dirty;

    /**
     * The actual bytes of data.
     */
    uint8_t data[BLOCK_SIZE];
};

/**
 * A directory contains a list of directory entries.
 */
struct DirectoryEntry {
    /**
     * True if the in-memory directory entry changed and needs to be written to disk.
     */
    bool dirty;

    /**
     * Inode number of entry
     */
    uint64_t inodeNumber;

    /**
     * Filename of entry
     */
    String filename;

    /**
     * Compare DirectoryEntries for inequality. (needed for ArrayList)
     */
    bool operator!=(const DirectoryEntry &other) {
        return inodeNumber != other.inodeNumber 
            || filename != other.filename;
    }
};

class Lfs
{
private:
    /**
     * The device this filesystem is read from or written to.
     */
    StorageDevice *device = nullptr;

    /**
     * True if device currently contains a valid lfs.
     */
    bool valid;

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
     * In-memory cache of blocks.
     * 
     * Maps a block number to its data block.
     */
    Util::HashMap<uint64_t, DataBlock> blockCache;

     /**
     * In-memory cache of directory entries.
     * 
     * Maps inode number to its directory entries.
     */
    Util::HashMap<uint64_t, Util::Array<DirectoryEntry>> directoryEntryCache;

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
     * Initilize caches with data from disk
     * 
     * @return false if disk does not conatin a valid lfs
     */
    bool readLfsFromDevice();

    /**
     * Read a data block.
     * 
     * @return DataBlock for blockNumber
     */
    DataBlock getDataBlock(uint64_t blockNumber);

    /**
     * Read a data block realtive to file.
     * 
     * @return DataBlock for blockNumber from inodes data block tables
     */
    DataBlock getDataBlockFromFile(Inode &inode, uint64_t blockNumber);

    /**
     * Write a data block realtive to file into block cache.
     * 
     */
    void setDataBlockFromFile(Inode &inode, uint64_t blockNumber, DataBlock block);

     /**
     * Reads all directory entries of a directory.
     * 
     * @return an array of directory entries
     */
    Util::Array<DirectoryEntry> readDirectoryEntries(Inode &dir);

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

public:
    /**
     * Constructor.
     */
    Lfs(StorageDevice *device);

    /**
     * Destructor.
     */
    ~Lfs();

    /**
     * Resets the current lfs to be a fresh empty one.
     */
    void reset();

    /**
     * Forces all caches to be written to disk.
     * 
     * @return true if successful
     */
    bool flush();

    /**
     * Read bytes from the file's data.
     * If (pos + numBytes) is greater than the data's length, END_OF_FILE shall be appended.
     * If pos is greater than the data's length, END_OF_FILE shall be the first and only character,
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
     * Write bytes to the file's data. If the offset points right into the existing data,
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
     * Get the length (in bytes) of the file's data.
     * If the node is a directory, this should always return 0.
     * 
     * @param path The path to a file
     */
    uint64_t getLength(const String &path);

    /**
     * Get the files's children.
     * 
     * @param path The path to a file
     */
    Util::Array<String> getChildren(const String &path);

    /**
     * Get if device contains a valid lfs.
     */
    bool isValid();
};

#endif