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
    // alloc buffers
    blockBuffer = new uint8_t[BLOCK_SIZE];
    segmentBuffer = new uint8_t[SEGMENT_SIZE];

    sectorsPerBlock = BLOCK_SIZE / device->getSectorSize();
    valid = readLfsFromDevice();
}

Lfs::~Lfs()
{
    flush();

    delete[] blockBuffer;
    delete[] segmentBuffer;
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

void Lfs::flush()
{
    // write directory entries
    Util::Array<uint64_t> inodeNumbers = directoryEntryCache.keySet();

    for(int i = 0; i < inodeNumbers.length(); i++) {
        uint64_t n = inodeNumbers[i];
        Util::Array<DirectoryEntry> entries = directoryEntryCache.get(n);
        Inode inode = inodeCache.get(n);

        // remove all blocks from inode
        // TODO remove blocks from block cache?
        for(int j = 0; j < 10; j++) {
            inode.directBlocks[j] = 0;
        }
        inode.indirectBlocks = 0;
        inode.doublyIndirectBlocks = 0;

        // TODO find a way that not all directory entries need to be rewritten

        // offset of next directory entry in current block
        uint64_t directoryEntryOffset = 0;
        uint64_t currentBlock = 0;

        for(int j = 0; j < entries.length(); j++) {
            DirectoryEntry entry = entries[j];

            // directory entries have varying sizes
            uint64_t entrySize = 8 + 4 + entry.filename.length();

            // write block if next directory entry does not fit
            if(BLOCK_SIZE - directoryEntryOffset < entrySize) {
                DataBlock block;
                block.dirty = true;
                memcpy(block.data, blockBuffer, BLOCK_SIZE);
                setDataBlockFromFile(inode, currentBlock, block);
                directoryEntryOffset = 0;
                currentBlock++;
            }

            // write directory entry
            Util::ByteBuffer::writeU64(blockBuffer, directoryEntryOffset + 0, entry.inodeNumber);
            Util::ByteBuffer::writeU32(blockBuffer, directoryEntryOffset + 8, entry.filename.length());
            Util::ByteBuffer::writeString(blockBuffer, directoryEntryOffset + 12, entry.filename);

            directoryEntryOffset += entrySize;
        }

        // write partial block
        DataBlock block;
        block.dirty = true;
        memcpy(block.data, blockBuffer, BLOCK_SIZE);
        //setDataBlockFromFile(inode, currentBlock, block);

        // update inode in cache
        inode.dirty = true;
        inodeCache.put(n, inode);
    }

    // write all dirty blocks
    Util::Array<uint64_t> blockNumbers = blockCache.keySet();
    int blockInSegment = 0;

    for(int i = 0; i < blockNumbers.length(); i++) {
        uint64_t n = blockNumbers[i];
        DataBlock block = blockCache.get(n);
        if(block.dirty) {
            
            // check if segment full
            if(blockInSegment * BLOCK_SIZE >= SEGMENT_SIZE) {
                // write out segment
                // calculate offset and size of segment. offset is incremented by one block for superblock
                device->write(segmentBuffer, superblock.currentSegment * BLOCKS_PER_SEGMENT * sectorsPerBlock + 1 * sectorsPerBlock, BLOCKS_PER_SEGMENT * sectorsPerBlock);

                superblock.currentSegment++;

                blockInSegment = 0;
            }

            // copy data to segment buffer
            memcpy(segmentBuffer + blockInSegment * BLOCK_SIZE, block.data, BLOCK_SIZE);

            blockInSegment++;

            // remove dirty flag
            block.dirty = false;
            blockCache.put(n, block);
        }
    }

    // write all dirty inodes
    inodeNumbers = inodeCache.keySet();

    // offset of next inode in current block
    uint32_t inodeOffset = 0;

    for(int i = 0; i < inodeNumbers.length(); i++) {
        uint64_t n = inodeNumbers[i];
        Inode inode = inodeCache.get(n);
        if(inode.dirty) {
            // write block if inode does not fit
            if(BLOCK_SIZE - inodeOffset < INODE_SIZE) {

                // check if segment full
                if(blockInSegment * BLOCK_SIZE >= SEGMENT_SIZE) {
                    // write out segment
                    // calculate offset and size of segment. offset is incremented by one block for superblock
                    device->write(segmentBuffer, superblock.currentSegment * BLOCKS_PER_SEGMENT * sectorsPerBlock + 1 * sectorsPerBlock, BLOCKS_PER_SEGMENT * sectorsPerBlock);

                    superblock.currentSegment++;

                    blockInSegment = 0;
                }

                // copy data to segment buffer
                memcpy(segmentBuffer + blockInSegment * BLOCK_SIZE, blockBuffer, BLOCK_SIZE);

                blockInSegment++;

                inodeOffset = 0;
            }

            // write inode
            Util::ByteBuffer::writeU64(blockBuffer, inodeOffset + 0, inode.size);
            Util::ByteBuffer::writeU8(blockBuffer, inodeOffset + 8, inode.fileType);
            Util::ByteBuffer::writeU64(blockBuffer, inodeOffset + 9, inode.directBlocks[0]);
            Util::ByteBuffer::writeU64(blockBuffer, inodeOffset + 17, inode.directBlocks[1]);
            Util::ByteBuffer::writeU64(blockBuffer, inodeOffset + 25, inode.directBlocks[2]);
            Util::ByteBuffer::writeU64(blockBuffer, inodeOffset + 33, inode.directBlocks[3]);
            Util::ByteBuffer::writeU64(blockBuffer, inodeOffset + 41, inode.directBlocks[4]);
            Util::ByteBuffer::writeU64(blockBuffer, inodeOffset + 49, inode.directBlocks[5]);
            Util::ByteBuffer::writeU64(blockBuffer, inodeOffset + 57, inode.directBlocks[6]);
            Util::ByteBuffer::writeU64(blockBuffer, inodeOffset + 65, inode.directBlocks[7]);
            Util::ByteBuffer::writeU64(blockBuffer, inodeOffset + 73, inode.directBlocks[8]);
            Util::ByteBuffer::writeU64(blockBuffer, inodeOffset + 81, inode.directBlocks[9]);
            Util::ByteBuffer::writeU64(blockBuffer, inodeOffset + 89, inode.indirectBlocks);
            Util::ByteBuffer::writeU64(blockBuffer, inodeOffset + 97, inode.doublyIndirectBlocks);

            // update inode map with new inode position
            InodeMapEntry entry;
            entry.inodeOffset = inodeOffset;
            // current block number
            entry.inodePosition = superblock.currentSegment * BLOCKS_PER_SEGMENT + blockInSegment;

            inodeMap.put(n, entry);

            inodeOffset += INODE_SIZE;

            // remove dirty flag
            inode.dirty = false;
            inodeCache.put(n, inode);
        }

        // write partial block
        // check if segment full
        if(blockInSegment * BLOCK_SIZE >= SEGMENT_SIZE) {
            // write out segment
            // calculate offset and size of segment. offset is incremented by one block for superblock
            device->write(segmentBuffer, superblock.currentSegment * BLOCKS_PER_SEGMENT * sectorsPerBlock + 1 * sectorsPerBlock, BLOCKS_PER_SEGMENT * sectorsPerBlock);

            superblock.currentSegment++;

            blockInSegment = 0;
        }

        // copy data to segment buffer
        memcpy(segmentBuffer + blockInSegment * BLOCK_SIZE, blockBuffer, BLOCK_SIZE);
        blockInSegment++;
    }

    // write inode map

    // current block number
    superblock.inodeMapPosition = superblock.currentSegment * BLOCKS_PER_SEGMENT + blockInSegment;
    superblock.inodeMapSize = 0;

    // offset of next inode map entry in current block
    uint32_t inodeMapEntryOffset = 0;

    inodeNumbers = inodeMap.keySet();
    for(int i = 0; i < inodeNumbers.length(); i++) {
        uint64_t n = inodeNumbers[i];
        InodeMapEntry entry = inodeMap.get(n);

        // write block if inode map entry does not fit
        if(BLOCK_SIZE - inodeMapEntryOffset < INODE_MAP_ENTRY_SIZE) {

            // check if segment full
            if(blockInSegment * BLOCK_SIZE >= SEGMENT_SIZE) {
                // write out segment
                // calculate offset and size of segment. offset is incremented by one block for superblock
                device->write(segmentBuffer, superblock.currentSegment * BLOCKS_PER_SEGMENT * sectorsPerBlock + 1 * sectorsPerBlock, BLOCKS_PER_SEGMENT * sectorsPerBlock);

                superblock.currentSegment++;

                blockInSegment = 0;
            }

            // copy data to segment buffer
            memcpy(segmentBuffer + blockInSegment * BLOCK_SIZE, blockBuffer, BLOCK_SIZE);

            blockInSegment++;

            inodeMapEntryOffset = 0;

            superblock.inodeMapSize++;
        }

        // write inode map entry
        Util::ByteBuffer::writeU64(blockBuffer, inodeMapEntryOffset, n);
        Util::ByteBuffer::writeU64(blockBuffer, inodeMapEntryOffset + 8, entry.inodePosition);
        Util::ByteBuffer::writeU32(blockBuffer, inodeMapEntryOffset + 16, entry.inodeOffset);

        inodeMapEntryOffset += INODE_MAP_ENTRY_SIZE;
    }

    // write partial block
    // check if segment full
    if(blockInSegment * BLOCK_SIZE >= SEGMENT_SIZE) {
        // write out segment
        // calculate offset and size of segment. offset is incremented by one block for superblock
        device->write(segmentBuffer, superblock.currentSegment * BLOCKS_PER_SEGMENT * sectorsPerBlock + 1 * sectorsPerBlock, BLOCKS_PER_SEGMENT * sectorsPerBlock);

        superblock.currentSegment++;

        blockInSegment = 0;
    }

    // copy data to segment buffer
    memcpy(segmentBuffer + blockInSegment * BLOCK_SIZE, blockBuffer, BLOCK_SIZE);
    blockInSegment++;
    superblock.inodeMapSize++;

    // write partial segment
    // calculate offset and size of segment. offset is incremented by one block for superblock
    device->write(segmentBuffer, superblock.currentSegment * BLOCKS_PER_SEGMENT * sectorsPerBlock + 1  * sectorsPerBlock, BLOCKS_PER_SEGMENT * sectorsPerBlock);

    superblock.currentSegment++;

    // write superblock
    Util::ByteBuffer::writeU32(blockBuffer, 0, superblock.magic);
    Util::ByteBuffer::writeU64(blockBuffer, 4, superblock.inodeMapPosition);
    Util::ByteBuffer::writeU64(blockBuffer, 12, superblock.inodeMapSize);
    Util::ByteBuffer::writeU64(blockBuffer, 20, superblock.currentSegment); 

    device->write(blockBuffer, 0, sectorsPerBlock);

    // reset segment buffer
    nextBlockInSegment = 0;
}

DataBlock Lfs::getDataBlockFromFile(Inode &inode, uint64_t blockNumber) {
    // TODO block number could be zero
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
        // if there is non yet, allocate new block
        if(inode.directBlocks[blockNumber] == 0) {
            inode.directBlocks[blockNumber] = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment;
            nextBlockInSegment++;
        }

        return blockCache.put(inode.directBlocks[blockNumber], block);
    } else if(blockNumber < 10 + BLOCKS_PER_INDIRECT_BLOCK) {

        DataBlock indirectBlock;

        // if there is no indirect block yet, allocate new block
        if(inode.indirectBlocks == 0) {
            inode.indirectBlocks = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment;
            nextBlockInSegment++;

            // make indirect block empty
            memset(indirectBlock.data, 0, BLOCK_SIZE);
            indirectBlock.dirty = true;

            // add to cache
            blockCache.put(inode.indirectBlocks, indirectBlock);
        } else {
            indirectBlock = getDataBlock(inode.indirectBlocks);
        }


        uint64_t indirectBlockNumber = Util::ByteBuffer::readU64(indirectBlock.data, (blockNumber - 10) * sizeof(uint64_t));
        // if there is non yet, allocate new block
        if(indirectBlockNumber == 0) {
            indirectBlockNumber = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment;
            nextBlockInSegment++;

            // write newly allocated number to indirect block
            Util::ByteBuffer::writeU64(indirectBlock.data, (blockNumber - 10) * sizeof(uint64_t), indirectBlockNumber);
            indirectBlock.dirty = true;
            blockCache.put(inode.indirectBlocks, indirectBlock);
        }

        return blockCache.put(indirectBlockNumber, block);
    } else {

        DataBlock doublyIndirectBlock;

        // if there is no doubly indirect block yet, allocate new block
        if(inode.doublyIndirectBlocks == 0) {
            inode.doublyIndirectBlocks = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment;
            nextBlockInSegment++;

            // make indirect block empty
            memset(doublyIndirectBlock.data, 0, BLOCK_SIZE);
            doublyIndirectBlock.dirty = true;

            // add to cache
            blockCache.put(inode.doublyIndirectBlocks, doublyIndirectBlock);

        } else {
            doublyIndirectBlock = getDataBlock(inode.doublyIndirectBlocks);
        }

        uint64_t n = blockNumber - 10 - BLOCKS_PER_INDIRECT_BLOCK;
        uint64_t j = n / BLOCKS_PER_INDIRECT_BLOCK;
        uint64_t i = n % BLOCKS_PER_INDIRECT_BLOCK;

        uint64_t doublyIndirectBlockNumber = Util::ByteBuffer::readU64(doublyIndirectBlock.data, j * sizeof(uint64_t));

        DataBlock indirectBlock;

        // if there is no indirect block yet, allocate new block
        if(doublyIndirectBlockNumber == 0) {
            doublyIndirectBlockNumber = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment;
            nextBlockInSegment++;

            // make indirect block empty
            memset(indirectBlock.data, 0, BLOCK_SIZE);
            indirectBlock.dirty = true;

            // add to cache
            blockCache.put(doublyIndirectBlockNumber, indirectBlock);

            // write newly allocated number to doubly indirect block
            Util::ByteBuffer::writeU64(doublyIndirectBlock.data, j * sizeof(uint64_t), doublyIndirectBlockNumber);
            doublyIndirectBlock.dirty = true;
            blockCache.put(inode.doublyIndirectBlocks, doublyIndirectBlock);

        } else {
            indirectBlock = getDataBlock(doublyIndirectBlockNumber);
        }

        uint64_t indirectBlockNumber = Util::ByteBuffer::readU64(indirectBlock.data, i * sizeof(uint64_t));

        // if there is non yet, allocate new block
        if(indirectBlockNumber == 0) {
            indirectBlockNumber = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment;
            nextBlockInSegment++;

            // write newly allocated number to indirect block
            Util::ByteBuffer::writeU64(indirectBlock.data, i * sizeof(uint64_t), indirectBlockNumber);
            indirectBlock.dirty = true;
            blockCache.put(doublyIndirectBlockNumber, indirectBlock);
        }

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

    // updated inode needs to be rewritten to disk
    inode.dirty = true;

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

    // update file size
    inode.size += length;

    // update inode in cache
    inodeCache.put(inodeNumber, inode);

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

        // block id 0 is unused entry
        if(inode.directBlocks[i] != 0) {
            blockCache.remove(inode.directBlocks[i]);
        }        
    }

    // indirect blocks
    if(inode.indirectBlocks != 0) {
        DataBlock indirectBlock = getDataBlock(inode.indirectBlocks);

        for(size_t i = 0; i < BLOCK_SIZE / sizeof(uint64_t); i++) {
            uint64_t blockNumber = Util::ByteBuffer::readU64(indirectBlock.data, i * sizeof(uint64_t));

            // block id 0 is unused entry
            if(blockNumber != 0) {
                blockCache.remove(blockNumber);
            }
        }

        // remove indirect block itself
        blockCache.remove(inode.indirectBlocks);
    }

    // doubly indirect blocks
    if(inode.doublyIndirectBlocks != 0) {
        DataBlock doublyIndirectBlock = getDataBlock(inode.doublyIndirectBlocks);

        for(size_t j = 0; j < BLOCK_SIZE / sizeof(uint64_t); j++) {
            uint64_t indirectBlockNumber = Util::ByteBuffer::readU64(doublyIndirectBlock.data, j * sizeof(uint64_t));

            // block id 0 is unused entry
            if(indirectBlockNumber != 0) {

                DataBlock indirectBlock = getDataBlock(indirectBlockNumber);

                for(size_t i = 0; i < BLOCK_SIZE / sizeof(uint64_t); i++) {
                    uint64_t blockNumber = Util::ByteBuffer::readU64(indirectBlock.data, i * sizeof(uint64_t));

                    // block id 0 is unused entry
                    if(blockNumber != 0) {
                        blockCache.remove(blockNumber);
                    }
                }

                // remove indirect block itself
                blockCache.remove(indirectBlockNumber);
            }
        }

        // remove doubly indirect block itself
        blockCache.remove(inode.doublyIndirectBlocks);
    }

    inodeCache.remove(inodeNumber);
    inodeMap.remove(inodeNumber);

    // delete directory entry in parent
    uint64_t parentInodeNumber = getParentInodeNumber(path);
    Inode parentInode = getInode(parentInodeNumber);
    Util::ArrayList<DirectoryEntry> oldParentDirectoryEntries(readDirectoryEntries(parentInode));
    Util::ArrayList<DirectoryEntry> newParentDirectoryEntries;

    // find current file entry and update it
    for(size_t i = 0; i < oldParentDirectoryEntries.size(); i++) {
        DirectoryEntry entry = oldParentDirectoryEntries.get(i);

        if(entry.inodeNumber == inodeNumber) {
            // TODO directory entries need size for deletion
            // remove entry by setting its inodeNumber invalid
            entry.inodeNumber = 0;
        }

        newParentDirectoryEntries.add(entry);
    }

    directoryEntryCache.put(parentInodeNumber, newParentDirectoryEntries.toArray());

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

        // read block that contains inode
        uint8_t inodeBuffer[BLOCK_SIZE];
        device->read(inodeBuffer, entry.inodePosition, BLOCK_SIZE);

        // read inode data at offset
        Inode inode;
        inode.size = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 0);
        inode.fileType = Util::ByteBuffer::readU8(inodeBuffer, entry.inodeOffset + 8);
        inode.directBlocks[0] = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 9);
        inode.directBlocks[1] = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 17);
        inode.directBlocks[2] = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 25);
        inode.directBlocks[3] = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 33);
        inode.directBlocks[4] = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 41);
        inode.directBlocks[5] = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 49);
        inode.directBlocks[6] = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 57);
        inode.directBlocks[7] = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 65);
        inode.directBlocks[8] = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 73);
        inode.directBlocks[9] = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 81);
        inode.indirectBlocks = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 89);
        inode.doublyIndirectBlocks = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 97);

        // add to cache
        inodeCache.put(inodeNumber, inode);

        return inode;
    }
}

bool Lfs::readLfsFromDevice()
{
    // first block contains information about filesystem
    device->read(blockBuffer, 0, sectorsPerBlock);

    superblock.magic = Util::ByteBuffer::readU32(blockBuffer, 0);

    if (superblock.magic != LFS_MAGIC)
    {
        return false;
    }

    superblock.inodeMapPosition = Util::ByteBuffer::readU64(blockBuffer, 4);
    superblock.inodeMapSize = Util::ByteBuffer::readU64(blockBuffer, 12);
    superblock.currentSegment = Util::ByteBuffer::readU64(blockBuffer, 20);

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

        // block id 0 is unused entry
        if(dir.directBlocks[i] == 0) {
            continue;
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

             // block id 0 is unused entry
            if(blockNumber == 0) {
                continue;
            }

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

             // block id 0 is unused entry
            if(indirectBlockNumber == 0) {
                continue;
            }

            DataBlock indirectBlock = getDataBlock(indirectBlockNumber);

            for(size_t i = 0; i < BLOCK_SIZE / sizeof(uint64_t); i++) {
                uint64_t blockNumber = Util::ByteBuffer::readU64(indirectBlock.data, i * sizeof(uint64_t));

                // block id 0 is unused entry
                if(blockNumber == 0) {
                    continue;
                }

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
