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

#include "Lfs.h"
#include "filesystem/core/Filesystem.h"

Lfs::Lfs(StorageDevice *device) : device(device)
{
    sectorsPerBlock = BLOCK_SIZE / device->getSectorSize();
    valid = readLfsFromDevice();
}

Lfs::~Lfs()
{
    flush();
}

void Lfs::reset()
{
    // reset super block
    superblock.magic = LFS_MAGIC;
    superblock.inodeMapPosition = 0;
    superblock.inodeMapSize = 0;
    superblock.currentSegment = 0;

    // reset caches
    inodeMap.clear();
    inodeCache.clear();
    blockCache.clear();

    // add empty root dir; root dir is always inode number 1
    // TODO add . and .. (both pointing to / itself)
    Inode rootDir = {
        .dirty = true,
        .size = 0,
        .fileType = FsNode::FileType::DIRECTORY_FILE,
        .directBlocks = {0},
        .indirectBlocks = 0,
        .doublyIndirectBlocks = 0,
    };
    inodeCache.put(1, rootDir);
}

bool Lfs::flush()
{
    // TODO
    return false;
}

uint64_t Lfs::readData(const String &path, char *buf, uint64_t pos, uint64_t numBytes)
{
    // TODO
    return 0;
}

uint64_t Lfs::writeData(const String &path, char *buf, uint64_t pos, uint64_t length)
{
    // TODO
    return 0;
}

bool Lfs::createNode(const String &path, uint8_t fileType)
{
    // TODO
    return false;
}

bool Lfs::deleteNode(const String &path)
{
    // TODO
    return false;
}

uint8_t Lfs::getFileType(const String &path)
{
    uint64_t inodeNumber = getInodeNumber(path);

    if (inodeNumber == 0)
    {
        return 0;
    }

    Inode inode = getInode(inodeNumber);

    return inode.fileType;
}

uint64_t Lfs::getLength(const String &path)
{
    uint64_t inodeNumber = getInodeNumber(path);

    if (inodeNumber == 0)
    {
        return 0;
    }

    Inode inode = getInode(inodeNumber);

    return inode.size;
}

Util::Array<String> Lfs::getChildren(const String &path)
{
    uint64_t inodeNumber = getInodeNumber(path);

    if(inodeNumber == 0) {
        return Util::Array<String>(0);
    }

    Inode inode = getInode(inodeNumber);

    if(inode.fileType != FsNode::FileType::DIRECTORY_FILE) {
        return Util::Array<String>(0);
    }

    Util::Array<DirectoryEntry> directoryEntries = readDirectoryEntries(inode);

    Util::Array<String> res = Util::Array<String>(directoryEntries.length());
    for(size_t i = 0; i < directoryEntries.length(); i++) {
        res[i] = directoryEntries[i].filename;
    }

    return res;
}

bool Lfs::isValid()
{
    return valid;
}

uint32_t Lfs::readU32(uint8_t *buffer, size_t offset)
{
    return ((uint32_t)buffer[offset]) | (((uint32_t)buffer[offset + 1]) << 8) | (((uint32_t)buffer[offset + 2]) << 16) | (((uint32_t)buffer[offset + 3]) << 24);
}

uint64_t Lfs::readU64(uint8_t *buffer, size_t offset)
{
    return ((uint64_t)buffer[offset]) | (((uint64_t)buffer[offset + 1]) << 8) | (((uint64_t)buffer[offset + 2]) << 16) | (((uint64_t)buffer[offset + 3]) << 24) | (((uint64_t)buffer[offset + 4]) << 32) | (((uint64_t)buffer[offset + 5]) << 40) | (((uint64_t)buffer[offset + 6]) << 48) | (((uint64_t)buffer[offset + 7]) << 56);
}

uint64_t Lfs::getInodeNumber(const String &path)
{
    // split the path to search left to right
    Util::Array<String> token = path.split(Filesystem::SEPARATOR);

    // starting at root directory (always inode 1)
    uint64_t currentInodeNumber = 1;

    for(size_t i = 0; i < token.length(); i++) {
        // find token[i] in currentDir
        Inode currentDir = getInode(currentInodeNumber);

        Util::Array<DirectoryEntry> directoryEntries = readDirectoryEntries(currentDir);

        // check if token[i] is in current dir
        bool found = false;
        for(size_t i = 0; i < directoryEntries.length(); i++) {
            if(directoryEntries[i].filename == token[i]) {
                currentInodeNumber = directoryEntries[i].inodeNumber;
                found = true;
                break;
            }
        }

        if(!found) {
            return 0;
        }
    }

    return currentInodeNumber;
}

Inode Lfs::getInode(uint64_t inodeNumber)
{
    // check if inode is cached
    if(inodeCache.containsKey(inodeNumber)) {
        return inodeCache.get(inodeNumber);
    } else {
        // else load it from disk
        InodeMapEntry entry = inodeMap.get(inodeNumber);

        uint8_t inodeBuffer[BLOCK_SIZE];
        device->read(inodeBuffer, entry.inodePosition, BLOCK_SIZE);

        Inode inode;
        inode.size = readU64(inodeBuffer, 0);
        inode.fileType = inodeBuffer[8];
        inode.directBlocks[0] = readU64(inodeBuffer, 9);
        inode.directBlocks[1] = readU64(inodeBuffer, 17);
        inode.directBlocks[2] = readU64(inodeBuffer, 25);
        inode.directBlocks[3] = readU64(inodeBuffer, 33);
        inode.directBlocks[4] = readU64(inodeBuffer, 41);
        inode.directBlocks[5] = readU64(inodeBuffer, 49);
        inode.directBlocks[6] = readU64(inodeBuffer, 57);
        inode.directBlocks[7] = readU64(inodeBuffer, 65);
        inode.directBlocks[8] = readU64(inodeBuffer, 73);
        inode.directBlocks[9] = readU64(inodeBuffer, 81);
        inode.indirectBlocks = readU64(inodeBuffer, 89);
        inode.doublyIndirectBlocks = readU64(inodeBuffer, 97);

        // add to cache
        inodeCache.put(inodeNumber, inode);

        return inode;
    }
}

bool Lfs::readLfsFromDevice()
{
    // first block contains information about filesystem
    uint8_t superblockBuffer[BLOCK_SIZE];
    device->read(superblockBuffer, 0, sectorsPerBlock);

    superblock.magic = readU32(superblockBuffer, 0);

    if (superblock.magic != LFS_MAGIC)
    {
        return false;
    }

    superblock.inodeMapPosition = readU64(superblockBuffer, 4);
    superblock.inodeMapSize = readU64(superblockBuffer, 12);
    superblock.currentSegment = readU64(superblockBuffer, 20);

    uint8_t inodeMapBuffer[BLOCK_SIZE * superblock.inodeMapSize];
    device->read(inodeMapBuffer, superblock.inodeMapPosition * sectorsPerBlock, superblock.inodeMapSize * sectorsPerBlock);

    for (size_t i = 0; i < BLOCK_SIZE * superblock.inodeMapSize; i += 20)
    {
        uint64_t inodeNum = readU64(inodeMapBuffer, i);

        if (inodeNum == 0)
        {
            break;
        }

        InodeMapEntry entry;

        entry.inodePosition = readU64(inodeMapBuffer, i + 8);
        entry.inodeOffset = readU32(inodeMapBuffer, i + 16);

        inodeMap.put(inodeNum, entry);
    }

    return true;
}

DataBlock Lfs::getDataBlock(uint64_t blockNumber) {
    // if not in cache read forom disk
    if(!blockCache.containsKey(blockNumber)) {
        DataBlock block;
        device->read(block.data, blockNumber * sectorsPerBlock, sectorsPerBlock);
        blockCache.put(blockNumber, block);
    }

    return blockCache.get(blockNumber);
}

Util::Array<DirectoryEntry> Lfs::readDirectoryEntries(Inode &dir) {
    Util::ArrayList<DirectoryEntry> entries;

    // read all directory entries in direct blocks
    for(size_t i = 0; i < 10; i++) {

        // block id 0 is end of list
        if(dir.directBlocks[i] == 0) {
            break;
        }

        DataBlock block = getDataBlock(dir.directBlocks[i]);

        // read all entries of block
        for(size_t d = 0; d < BLOCK_SIZE;) {
            DirectoryEntry entry;

            entry.inodeNumber = readU64(block.data, d);

            uint32_t filenameLength = readU32(block.data, d + sizeof(entry.inodeNumber));
            for(size_t l = 0; l < filenameLength; l++) {
                entry.filename += String(block.data[d + sizeof(entry.inodeNumber) + sizeof(filenameLength) + l]);
            }

            entries.add(entry);

            d += entry.filename.length() + sizeof(filenameLength) + sizeof(entry.inodeNumber);
        }
    }

    // read all directory entries in indirect blocks
    if(dir.indirectBlocks != 0) {
        DataBlock indirectBlock = getDataBlock(dir.indirectBlocks);

        for(size_t i = 0; i < BLOCK_SIZE / sizeof(uint64_t); i++) {
            uint64_t blockNumber = readU64(indirectBlock.data, i * sizeof(uint64_t));
            DataBlock block = getDataBlock(blockNumber);

            // read all entries of block
            for(size_t d = 0; d < BLOCK_SIZE;) {
                DirectoryEntry entry;

                entry.inodeNumber = readU64(block.data, d);

                uint32_t filenameLength = readU32(block.data, d + sizeof(entry.inodeNumber));
                for(size_t l = 0; l < filenameLength; l++) {
                    entry.filename += String(block.data[d + sizeof(entry.inodeNumber) + sizeof(filenameLength) + l]);
                }

                entries.add(entry);

                d += entry.filename.length() + sizeof(filenameLength) + sizeof(entry.inodeNumber);
            }
        }
    }

    // read all directory entries in doubly indirect blocks
    if(dir.doublyIndirectBlocks != 0) {
        DataBlock doublyIndirectBlock = getDataBlock(dir.doublyIndirectBlocks);

        for(size_t j = 0; j < BLOCK_SIZE / sizeof(uint64_t); j++) {
            uint64_t indirectBlockNumber = readU64(doublyIndirectBlock.data, j * sizeof(uint64_t));
            DataBlock indirectBlock = getDataBlock(indirectBlockNumber);

            for(size_t i = 0; i < BLOCK_SIZE / sizeof(uint64_t); i++) {
                uint64_t blockNumber = readU64(indirectBlock.data, i * sizeof(uint64_t));
                DataBlock block = getDataBlock(blockNumber);

                // read all entries of block
                for(size_t d = 0; d < BLOCK_SIZE;) {
                    DirectoryEntry entry;

                    entry.inodeNumber = readU64(block.data, d);

                    uint32_t filenameLength = readU32(block.data, d + sizeof(entry.inodeNumber));
                    for(size_t l = 0; l < filenameLength; l++) {
                        entry.filename += String(block.data[d + sizeof(entry.inodeNumber) + sizeof(filenameLength) + l]);
                    }

                    entries.add(entry);

                    d += entry.filename.length() + sizeof(filenameLength) + sizeof(entry.inodeNumber);
                }
            }
        }
    }

    return entries.toArray();
}

