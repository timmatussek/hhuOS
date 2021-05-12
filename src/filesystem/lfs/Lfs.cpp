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
#include "lib/util/ByteBuffer.h"
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
    Inode rootDir = {
        .dirty = true,
        .size = 0,
        .fileType = FsNode::FileType::DIRECTORY_FILE,
        .directBlocks = {0},
        .indirectBlocks = 0,
        .doublyIndirectBlocks = 0,
    };
    inodeCache.put(1, rootDir);

    // add . and .. (both pointing to / itself)
    Util::ArrayList<DirectoryEntry> entries;

    DirectoryEntry entryCurrentDir;
    entryCurrentDir.filename = ".";
    entryCurrentDir.inodeNumber = 1;

    entries.add(entryCurrentDir);

    DirectoryEntry entryParentDir;
    entryParentDir.filename = "..";
    entryParentDir.inodeNumber = 1;

    entries.add(entryParentDir);

    directoryEntryCache.put(1, entries.toArray());

    // as root dir is inode 1, next free inode is 2
    nextInodeNumber = 2;
}

bool Lfs::flush()
{
    /* 
        TODO 
        write all dirty blocks
        write all dirty inodes
        update indode map with new inode positions
        write inode map
        write superblock
    */
    return false;
}

DataBlock Lfs::getDataBlockFromFile(Inode &inode, uint64_t blockNumber) {
    // determine if block is in direct, indirect or doubly indirect list
    if(blockNumber < 10) {
        return getDataBlock(inode.directBlocks[blockNumber]);
    } else if(blockNumber < 10 + BLOCKS_PER_INDIRECT_BLOCK) {
        DataBlock indirectBlock = getDataBlock(inode.indirectBlocks);
        uint64_t indirectBlockNumber = Util::ByteBuffer::readU64(indirectBlock.data, (blockNumber - 10) * sizeof(uint64_t));
        return getDataBlock(indirectBlockNumber);
    } else {
        DataBlock doublyIndirectBlock = getDataBlock(inode.doublyIndirectBlocks);

        uint64_t n = blockNumber - 10 - BLOCKS_PER_INDIRECT_BLOCK;
        uint64_t j = n / BLOCKS_PER_INDIRECT_BLOCK;
        uint64_t i = n % BLOCKS_PER_INDIRECT_BLOCK;

        uint64_t doublyIndirectBlockNumber = Util::ByteBuffer::readU64(doublyIndirectBlock.data, j * sizeof(uint64_t));
        DataBlock indirectBlock = getDataBlock(doublyIndirectBlockNumber);
        uint64_t indirectBlockNumber = Util::ByteBuffer::readU64(indirectBlock.data, i * sizeof(uint64_t));
        return getDataBlock(indirectBlockNumber);
    }
}

void Lfs::setDataBlockFromFile(Inode &inode, uint64_t blockNumber, DataBlock block) {
    // determine if block is in direct, indirect or doubly indirect list
    if(blockNumber < 10) {
        return blockCache.put(inode.directBlocks[blockNumber], block);
    } else if(blockNumber < 10 + BLOCKS_PER_INDIRECT_BLOCK) {
        DataBlock indirectBlock = getDataBlock(inode.indirectBlocks);
        uint64_t indirectBlockNumber = Util::ByteBuffer::readU64(indirectBlock.data, (blockNumber - 10) * sizeof(uint64_t));
        return blockCache.put(indirectBlockNumber, block);
    } else {
        DataBlock doublyIndirectBlock = getDataBlock(inode.doublyIndirectBlocks);

        uint64_t n = blockNumber - 10 - BLOCKS_PER_INDIRECT_BLOCK;
        uint64_t j = n / BLOCKS_PER_INDIRECT_BLOCK;
        uint64_t i = n % BLOCKS_PER_INDIRECT_BLOCK;

        uint64_t doublyIndirectBlockNumber = Util::ByteBuffer::readU64(doublyIndirectBlock.data, j * sizeof(uint64_t));
        DataBlock indirectBlock = getDataBlock(doublyIndirectBlockNumber);
        uint64_t indirectBlockNumber = Util::ByteBuffer::readU64(indirectBlock.data, i * sizeof(uint64_t));
        return blockCache.put(indirectBlockNumber, block);
    }
}

uint64_t Lfs::readData(const String &path, char *buf, uint64_t pos, uint64_t numBytes)
{
    uint64_t inodeNumber = getInodeNumber(path);

    if (inodeNumber == 0)
    {
        return 0;
    }

    Inode inode = getInode(inodeNumber);

    uint64_t roundedPosition = roundUpBlockAddress(pos);
    uint64_t roundedLength  = roundDownBlockAddress(numBytes);

    uint64_t start = roundedPosition / BLOCK_SIZE;
    uint64_t end = (roundedPosition + roundedLength) / BLOCK_SIZE;

    for(size_t i = start; i < end; i++) {
        DataBlock block = getDataBlockFromFile(inode, i);
        memcpy(buf + i * BLOCK_SIZE, block.data, BLOCK_SIZE);
    }

    // if pos is not block aligned we need to read a partial block
    uint64_t startBlock = pos / BLOCK_SIZE;
    uint64_t startOffset = roundedPosition - pos;

    if(startOffset != 0) {
        DataBlock partialStartblock = getDataBlockFromFile(inode, startBlock);
        memcpy(buf + startBlock * BLOCK_SIZE, partialStartblock.data, startOffset);
    }

    // if numBytes is not block aligned we need to read a partial block
    uint64_t endBlock = (pos + numBytes) / BLOCK_SIZE;
    uint64_t endOffset = numBytes - roundedLength;

    if(endOffset != 0) {
        DataBlock partialEndBlock = getDataBlockFromFile(inode, endBlock);
        memcpy(buf + endBlock * BLOCK_SIZE, partialEndBlock.data, endOffset);
    }

    return numBytes;
}

uint64_t Lfs::writeData(const String &path, char *buf, uint64_t pos, uint64_t length)
{
    uint64_t inodeNumber = getInodeNumber(path);

    if (inodeNumber == 0)
    {
        return 0;
    }

    Inode inode = getInode(inodeNumber);

    uint64_t roundedPosition = roundUpBlockAddress(pos);
    uint64_t roundedLength  = roundDownBlockAddress(length);

    uint64_t start = roundedPosition / BLOCK_SIZE;
    uint64_t end = (roundedPosition + roundedLength) / BLOCK_SIZE;

    for(size_t i = start; i < end; i++) {
        DataBlock block = getDataBlockFromFile(inode, i);
        memcpy(block.data, buf + i * BLOCK_SIZE, BLOCK_SIZE);
        block.dirty = true;
        // update in cache
        setDataBlockFromFile(inode, i, block);
    }

    // if pos is not block aligned we need to read and write a partial block
    uint64_t startBlock = pos / BLOCK_SIZE;
    uint64_t startOffset = roundedPosition - pos;

    if(startOffset != 0) {
        DataBlock partialStartblock = getDataBlockFromFile(inode, startBlock);
        memcpy(partialStartblock.data, buf + startBlock * BLOCK_SIZE, startOffset);
        partialStartblock.dirty = true;
        // update in cache
        setDataBlockFromFile(inode, startBlock, partialStartblock);
    }

    // if length is not block aligned we need to read and write a partial block
    uint64_t endBlock = (pos + length) / BLOCK_SIZE;
    uint64_t endOffset = length - roundedLength;

    if(endOffset != 0) {
        DataBlock partialEndBlock = getDataBlockFromFile(inode, endBlock);
        memcpy(partialEndBlock.data, buf + endBlock * BLOCK_SIZE, endOffset);
        partialEndBlock.dirty = true;
        // update in cache
        setDataBlockFromFile(inode, endBlock, partialEndBlock);
    }

    // TODO update file size

    return length;
}

bool Lfs::createNode(const String &path, uint8_t fileType)
{
    uint64_t inodeNumber = getInodeNumber(path);

    // check if already exists
    if (inodeNumber != 0)
    {
        return false;
    }

    Inode inode;

    inode.dirty = true;
    inode.fileType = fileType;

    // allocate new inode number
    inodeNumber = nextInodeNumber;
    nextInodeNumber++;

    inodeCache.put(inodeNumber, inode);

    // add directory entry in parent
    uint64_t parentInodeNumber = getParentInodeNumber(path);
    Inode parentInode = getInode(parentInodeNumber);
    Util::ArrayList<DirectoryEntry> parentDirectoryEntries(readDirectoryEntries(parentInode));

    DirectoryEntry currentFile;
    currentFile.filename = getFileName(path);
    currentFile.inodeNumber = inodeNumber;

    parentDirectoryEntries.add(currentFile);

    directoryEntryCache.put(parentInodeNumber, parentDirectoryEntries.toArray());

    // if fileType is directory add . and ..
    if(fileType == FsNode::FileType::DIRECTORY_FILE) {
        Util::ArrayList<DirectoryEntry> entries;

        DirectoryEntry entryCurrentDir;
        entryCurrentDir.filename = ".";
        entryCurrentDir.inodeNumber = inodeNumber;

        entries.add(entryCurrentDir);

        DirectoryEntry entryParentDir;
        entryParentDir.filename = "..";
        entryParentDir.inodeNumber = parentInodeNumber;

        entries.add(entryParentDir);

        directoryEntryCache.put(inodeNumber, entries.toArray());
    }

    return true;
}

bool Lfs::deleteNode(const String &path)
{
    uint64_t inodeNumber = getInodeNumber(path);

    if (inodeNumber == 0)
    {
        return false;
    }

    Inode inode = getInode(inodeNumber);

    // TODO refactor looping all blocks

    // delete all blocks from blockCache
    // direct blocks
    for(size_t i = 0; i < 10; i++) {

        // block id 0 is end of list
        if(inode.directBlocks[i] == 0) {
            break;
        }

        blockCache.remove(inode.directBlocks[i]);
    }

    // indirect blocks
    if(inode.indirectBlocks != 0) {
        DataBlock indirectBlock = getDataBlock(inode.indirectBlocks);

        for(size_t i = 0; i < BLOCK_SIZE / sizeof(uint64_t); i++) {
            uint64_t blockNumber = Util::ByteBuffer::readU64(indirectBlock.data, i * sizeof(uint64_t));

            // TODO stop at 0? see direct blocks

            blockCache.remove(blockNumber);
        }

        // remove indirect block itself
        blockCache.remove(inode.indirectBlocks);
    }

    // doubly indirect blocks
    if(inode.doublyIndirectBlocks != 0) {
        DataBlock doublyIndirectBlock = getDataBlock(inode.doublyIndirectBlocks);

        for(size_t j = 0; j < BLOCK_SIZE / sizeof(uint64_t); j++) {
            uint64_t indirectBlockNumber = Util::ByteBuffer::readU64(doublyIndirectBlock.data, j * sizeof(uint64_t));

            // TODO stop at 0? see direct blocks

            DataBlock indirectBlock = getDataBlock(indirectBlockNumber);

            for(size_t i = 0; i < BLOCK_SIZE / sizeof(uint64_t); i++) {
                uint64_t blockNumber = Util::ByteBuffer::readU64(indirectBlock.data, i * sizeof(uint64_t));

                // TODO stop at 0? see direct blocks

                blockCache.remove(blockNumber);
            }

            // remove indirect block itself
            blockCache.remove(indirectBlockNumber);
        }

        // remove doubly indirect block itself
        blockCache.remove(inode.doublyIndirectBlocks);
    }

    inodeCache.remove(inodeNumber);
    inodeMap.remove(inodeNumber);

    // delete directory entry in parent
    uint64_t parentInodeNumber = getParentInodeNumber(path);
    Inode parentInode = getInode(parentInodeNumber);
    Util::ArrayList<DirectoryEntry> parentDirectoryEntries(readDirectoryEntries(parentInode));

    // find index to remove
    size_t currentFileIndex = 0;
    for(size_t i = 0; i < parentDirectoryEntries.size(); i++) {
        DirectoryEntry entry = parentDirectoryEntries.get(i);
        if(entry.inodeNumber == inodeNumber) {
            currentFileIndex = i;
            break;
        }
    }

    parentDirectoryEntries.remove(currentFileIndex);

    directoryEntryCache.put(parentInodeNumber, parentDirectoryEntries.toArray());

    return true;
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

String Lfs::getFileName(const String &path) {
    Util::Array<String> token = path.split(Filesystem::SEPARATOR);
    return token[token.length() - 1];
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

uint64_t Lfs::getParentInodeNumber(const String &path) {

    // split the path
    Util::Array<String> token = path.split(Filesystem::SEPARATOR);

    String parentPath = "/";

    // rebuild path without last element
    for(size_t i = 0; i < token.length() - 1; i++) {
        parentPath += token[i] + "/";
    }

    return getInodeNumber(parentPath);
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

        // TODO this should use inodeOffset

        Inode inode;
        inode.size = Util::ByteBuffer::readU64(inodeBuffer, 0);
        inode.fileType = inodeBuffer[8];
        inode.directBlocks[0] = Util::ByteBuffer::readU64(inodeBuffer, 9);
        inode.directBlocks[1] = Util::ByteBuffer::readU64(inodeBuffer, 17);
        inode.directBlocks[2] = Util::ByteBuffer::readU64(inodeBuffer, 25);
        inode.directBlocks[3] = Util::ByteBuffer::readU64(inodeBuffer, 33);
        inode.directBlocks[4] = Util::ByteBuffer::readU64(inodeBuffer, 41);
        inode.directBlocks[5] = Util::ByteBuffer::readU64(inodeBuffer, 49);
        inode.directBlocks[6] = Util::ByteBuffer::readU64(inodeBuffer, 57);
        inode.directBlocks[7] = Util::ByteBuffer::readU64(inodeBuffer, 65);
        inode.directBlocks[8] = Util::ByteBuffer::readU64(inodeBuffer, 73);
        inode.directBlocks[9] = Util::ByteBuffer::readU64(inodeBuffer, 81);
        inode.indirectBlocks = Util::ByteBuffer::readU64(inodeBuffer, 89);
        inode.doublyIndirectBlocks = Util::ByteBuffer::readU64(inodeBuffer, 97);

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

    superblock.magic = Util::ByteBuffer::readU32(superblockBuffer, 0);

    if (superblock.magic != LFS_MAGIC)
    {
        return false;
    }

    superblock.inodeMapPosition = Util::ByteBuffer::readU64(superblockBuffer, 4);
    superblock.inodeMapSize = Util::ByteBuffer::readU64(superblockBuffer, 12);
    superblock.currentSegment = Util::ByteBuffer::readU64(superblockBuffer, 20);

    uint8_t inodeMapBuffer[BLOCK_SIZE * superblock.inodeMapSize];
    device->read(inodeMapBuffer, superblock.inodeMapPosition * sectorsPerBlock, superblock.inodeMapSize * sectorsPerBlock);

    for (size_t i = 0; i < BLOCK_SIZE * superblock.inodeMapSize; i += 20)
    {
        uint64_t inodeNum = Util::ByteBuffer::readU64(inodeMapBuffer, i);

        if (inodeNum == 0)
        {
            break;
        }

        InodeMapEntry entry;

        entry.inodePosition = Util::ByteBuffer::readU64(inodeMapBuffer, i + 8);
        entry.inodeOffset = Util::ByteBuffer::readU32(inodeMapBuffer, i + 16);

        inodeMap.put(inodeNum, entry);

        // keep track of highest in-use inode number
        if(inodeNum >= nextInodeNumber) {
            nextInodeNumber = inodeNum + 1;
        }
    }

    return true;
}

DataBlock Lfs::getDataBlock(uint64_t blockNumber) {
    // if not in cache read from disk
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

            entry.inodeNumber = Util::ByteBuffer::readU64(block.data, d);

            uint32_t filenameLength = Util::ByteBuffer::readU32(block.data, d + sizeof(entry.inodeNumber));
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
            uint64_t blockNumber = Util::ByteBuffer::readU64(indirectBlock.data, i * sizeof(uint64_t));

            // TODO stop at 0? see direct blocks

            DataBlock block = getDataBlock(blockNumber);

            // read all entries of block
            for(size_t d = 0; d < BLOCK_SIZE;) {
                DirectoryEntry entry;

                entry.inodeNumber = Util::ByteBuffer::readU64(block.data, d);

                uint32_t filenameLength = Util::ByteBuffer::readU32(block.data, d + sizeof(entry.inodeNumber));
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
            uint64_t indirectBlockNumber = Util::ByteBuffer::readU64(doublyIndirectBlock.data, j * sizeof(uint64_t));

            // TODO stop at 0? see direct blocks

            DataBlock indirectBlock = getDataBlock(indirectBlockNumber);

            for(size_t i = 0; i < BLOCK_SIZE / sizeof(uint64_t); i++) {
                uint64_t blockNumber = Util::ByteBuffer::readU64(indirectBlock.data, i * sizeof(uint64_t));
                DataBlock block = getDataBlock(blockNumber);

                // read all entries of block
                for(size_t d = 0; d < BLOCK_SIZE;) {
                    DirectoryEntry entry;

                    entry.inodeNumber = Util::ByteBuffer::readU64(block.data, d);

                    uint32_t filenameLength = Util::ByteBuffer::readU32(block.data, d + sizeof(entry.inodeNumber));
                    entry.filename = Util::ByteBuffer::readString(block.data, d + sizeof(entry.inodeNumber) + sizeof(filenameLength), filenameLength);

                    entries.add(entry);

                    d += entry.filename.length() + sizeof(filenameLength) + sizeof(entry.inodeNumber);
                }
            }
        }
    }

    return entries.toArray();
}

uint64_t Lfs::roundUpBlockAddress(uint64_t addr) {
    if(addr % BLOCK_SIZE == 0) {
        return addr;
    }
    return ((addr / BLOCK_SIZE) + 1) * BLOCK_SIZE;
}

uint64_t Lfs::roundDownBlockAddress(uint64_t addr) {
    if(addr % BLOCK_SIZE == 0) {
        return addr;
    }
    return (addr / BLOCK_SIZE) * BLOCK_SIZE;
}
