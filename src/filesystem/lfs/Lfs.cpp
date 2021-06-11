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

Lfs::Lfs(StorageDevice *device, bool mount)
{
    this->device = device;

    // sector size is only known at runtime
    sectorsPerBlock = BLOCK_SIZE / device->getSectorSize();

    // allocate buffers
    blockBuffer = new uint8_t[BLOCK_SIZE];
    segmentBuffer = new uint8_t[SEGMENT_SIZE];

    // initialize empty lfs 

    // initialize super block
    superblock.magic = LFS_MAGIC;
    superblock.inodeMapPosition = 0;
    superblock.inodeMapSize = 0;
    superblock.currentSegment = 0;

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

    dirty = true;

    if(mount) {
        // try to read lfs from disk

        // first block contains information about filesystem
        device->read(blockBuffer, 0, sectorsPerBlock);

        // do nothing if device does not contain lfs signature
        uint32_t magic = Util::ByteBuffer::readU32(blockBuffer, 0);
        if (superblock.magic != LFS_MAGIC)
        {
            return;
        }

        superblock.magic = magic;
        superblock.inodeMapPosition = Util::ByteBuffer::readU64(blockBuffer, 4);
        superblock.inodeMapSize = Util::ByteBuffer::readU64(blockBuffer, 12);
        superblock.currentSegment = Util::ByteBuffer::readU64(blockBuffer, 20);

        // reset caches and load inode map from disk
        inodeCache.clear();
        inodeMap.clear();
        directoryEntryCache.clear();

        // TODO alloc on heap instaed
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

        // as we are now in the state the disk is in
        // we do not need to flush until changes appear
        dirty = false;
    }
}

Lfs::~Lfs()
{
    delete[] blockBuffer;
    delete[] segmentBuffer;
}

void Lfs::flush()
{
    // do nothing if no changes occurred since last flush
    if(!dirty) {
        return;
    }

    // clear garbage out of buffer
    memset(blockBuffer, 0, BLOCK_SIZE);

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
        uint64_t nextBlockInFile = 0;

        for(int j = 0; j < entries.length(); j++) {
            DirectoryEntry entry = entries[j];

            // directory entries have varying sizes
            uint64_t entrySize = 8 + 4 + entry.filename.length();

            // write block if next directory entry does not fit
            if(BLOCK_SIZE - directoryEntryOffset < entrySize) {
                setBlockInFile(inode, nextBlockInFile, blockBuffer);
                // clear garbage out of buffer
                memset(blockBuffer, 0, BLOCK_SIZE);
                directoryEntryOffset = 0;
                nextBlockInFile++;
            }

            // write directory entry
            Util::ByteBuffer::writeU64(blockBuffer, directoryEntryOffset + 0, entry.inodeNumber);
            Util::ByteBuffer::writeU32(blockBuffer, directoryEntryOffset + 8, entry.filename.length());
            Util::ByteBuffer::writeString(blockBuffer, directoryEntryOffset + 12, entry.filename);

            directoryEntryOffset += entrySize;
        }

        // write partial block
        setBlockInFile(inode, nextBlockInFile, blockBuffer);
        // clear garbage out of buffer
        memset(blockBuffer, 0, BLOCK_SIZE);

        // update inode in cache
        inode.dirty = true;
        inodeCache.put(n, inode);
    }

    // write all dirty inodes
    // TODO can be written directly to segment buffer instead of blocks first
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
                if(nextBlockInSegment * BLOCK_SIZE >= SEGMENT_SIZE) {
                    // write out segment
                    // calculate offset and size of segment. offset is incremented by one block for superblock
                    device->write(segmentBuffer, superblock.currentSegment * BLOCKS_PER_SEGMENT * sectorsPerBlock + 1 * sectorsPerBlock, BLOCKS_PER_SEGMENT * sectorsPerBlock);

                    superblock.currentSegment++;

                    nextBlockInSegment = 0;
                }

                // copy data to segment buffer
                memcpy(segmentBuffer + nextBlockInSegment * BLOCK_SIZE, blockBuffer, BLOCK_SIZE);

                // clear garbage out of buffer
                memset(blockBuffer, 0, BLOCK_SIZE);

                nextBlockInSegment++;

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
            entry.inodePosition = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment + 1;

            inodeMap.put(n, entry);

            inodeOffset += INODE_SIZE;

            // remove dirty flag
            inode.dirty = false;
            inodeCache.put(n, inode);
        }

        // write partial block
        // check if segment full
        if(nextBlockInSegment * BLOCK_SIZE >= SEGMENT_SIZE) {
            // write out segment
            // calculate offset and size of segment. offset is incremented by one block for superblock
            device->write(segmentBuffer, superblock.currentSegment * BLOCKS_PER_SEGMENT * sectorsPerBlock + 1 * sectorsPerBlock, BLOCKS_PER_SEGMENT * sectorsPerBlock);

            superblock.currentSegment++;

            nextBlockInSegment = 0;
        }

        // copy data to segment buffer
        memcpy(segmentBuffer + nextBlockInSegment * BLOCK_SIZE, blockBuffer, BLOCK_SIZE);
        nextBlockInSegment++;

        // clear garbage out of buffer
        memset(blockBuffer, 0, BLOCK_SIZE);
    }

    // write inode map
    // TODO can be written directly to segment buffer instead of blocks first

    // current block number
    superblock.inodeMapPosition = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment + 1;
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
            if(nextBlockInSegment * BLOCK_SIZE >= SEGMENT_SIZE) {
                // write out segment
                // calculate offset and size of segment. offset is incremented by one block for superblock
                device->write(segmentBuffer, superblock.currentSegment * BLOCKS_PER_SEGMENT * sectorsPerBlock + 1 * sectorsPerBlock, BLOCKS_PER_SEGMENT * sectorsPerBlock);

                superblock.currentSegment++;

                nextBlockInSegment = 0;
            }

            // copy data to segment buffer
            memcpy(segmentBuffer + nextBlockInSegment * BLOCK_SIZE, blockBuffer, BLOCK_SIZE);

            // clear garbage out of buffer
            memset(blockBuffer, 0, BLOCK_SIZE);

            nextBlockInSegment++;

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
    if(nextBlockInSegment * BLOCK_SIZE >= SEGMENT_SIZE) {
        // write out segment
        // calculate offset and size of segment. offset is incremented by one block for superblock
        device->write(segmentBuffer, superblock.currentSegment * BLOCKS_PER_SEGMENT * sectorsPerBlock + 1 * sectorsPerBlock, BLOCKS_PER_SEGMENT * sectorsPerBlock);

        superblock.currentSegment++;

        nextBlockInSegment = 0;
    }

    // copy data to segment buffer
    memcpy(segmentBuffer + nextBlockInSegment * BLOCK_SIZE, blockBuffer, BLOCK_SIZE);

    // clear garbage out of buffer
    memset(blockBuffer, 0, BLOCK_SIZE);

    nextBlockInSegment++;
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

    // clear garbage out of buffer
    memset(blockBuffer, 0, BLOCK_SIZE);

    // reset segment buffer
    nextBlockInSegment = 0;
}

void Lfs::getBlockInFile(Inode &inode, uint64_t blockNumberInFile, uint8_t* buffer) {
    // determine if block is in direct, indirect or doubly indirect list
    if(blockNumberInFile < 10) {
        // find blocknumber and read block
        uint64_t blockNumber = inode.directBlocks[blockNumberInFile];

        if(blockNumber == 0) {
            return;
        }

        // read data from segment buffer if still in cache
        if(blockNumber > superblock.currentSegment * BLOCKS_PER_SEGMENT) {
            uint64_t cachedBlockNumber = blockNumber - superblock.currentSegment * BLOCKS_PER_SEGMENT - 1;
            memcpy(buffer, segmentBuffer + cachedBlockNumber * BLOCK_SIZE, BLOCK_SIZE);
        } else {
            device->read(buffer, blockNumber * sectorsPerBlock, sectorsPerBlock);
        }
    } else if(blockNumberInFile < 10 + BLOCKS_PER_INDIRECT_BLOCK) {

        if(inode.indirectBlocks == 0) {
            return;
        }

        // read indirect block to block buffer
        // read data from segment buffer if still in cache
        if(inode.indirectBlocks > superblock.currentSegment * BLOCKS_PER_SEGMENT) {
            uint64_t cachedBlockNumber = inode.indirectBlocks - superblock.currentSegment * BLOCKS_PER_SEGMENT - 1;
            memcpy(blockBuffer, segmentBuffer + cachedBlockNumber * BLOCK_SIZE, BLOCK_SIZE);
        } else {
            device->read(blockBuffer, inode.indirectBlocks * sectorsPerBlock, sectorsPerBlock);
        }

        // find blocknumber and read block
        uint64_t indirectBlockNumber = Util::ByteBuffer::readU64(blockBuffer, (blockNumberInFile - 10) * sizeof(uint64_t));

        if(indirectBlockNumber == 0) {
            return;
        }

        // read data from segment buffer if still in cache
        if(indirectBlockNumber > superblock.currentSegment * BLOCKS_PER_SEGMENT) {
            uint64_t cachedBlockNumber = indirectBlockNumber - superblock.currentSegment * BLOCKS_PER_SEGMENT - 1;
            memcpy(buffer, segmentBuffer + cachedBlockNumber * BLOCK_SIZE, BLOCK_SIZE);
        } else {
            device->read(buffer, indirectBlockNumber * sectorsPerBlock, sectorsPerBlock);
        }
    } else {

        if(inode.doublyIndirectBlocks == 0) {
            return;
        }

        // read doubly indirect block to block buffer
        // read data from segment buffer if still in cache
        if(inode.doublyIndirectBlocks > superblock.currentSegment * BLOCKS_PER_SEGMENT) {
            uint64_t cachedBlockNumber = inode.doublyIndirectBlocks - superblock.currentSegment * BLOCKS_PER_SEGMENT - 1;
            memcpy(blockBuffer, segmentBuffer + cachedBlockNumber * BLOCK_SIZE, BLOCK_SIZE);
        } else {
            device->read(blockBuffer, inode.doublyIndirectBlocks * sectorsPerBlock, sectorsPerBlock);
        }
        
        // calculate offsets into indirect blocks
        uint64_t n = blockNumberInFile - 10 - BLOCKS_PER_INDIRECT_BLOCK;
        uint64_t j = n / BLOCKS_PER_INDIRECT_BLOCK;
        uint64_t i = n % BLOCKS_PER_INDIRECT_BLOCK;

        // find blocknumber and read indirect block to buffer
        uint64_t doublyIndirectBlockNumber = Util::ByteBuffer::readU64(blockBuffer, j * sizeof(uint64_t));

        if(doublyIndirectBlockNumber == 0) {
            return;
        }

        // read data from segment buffer if still in cache
        if(doublyIndirectBlockNumber > superblock.currentSegment * BLOCKS_PER_SEGMENT) {
            uint64_t cachedBlockNumber = doublyIndirectBlockNumber - superblock.currentSegment * BLOCKS_PER_SEGMENT - 1;
            memcpy(blockBuffer, segmentBuffer + cachedBlockNumber * BLOCK_SIZE, BLOCK_SIZE);
        } else {
            device->read(blockBuffer, doublyIndirectBlockNumber * sectorsPerBlock, sectorsPerBlock);
        }

        // find blocknumber and read block
        uint64_t indirectBlockNumber = Util::ByteBuffer::readU64(blockBuffer, i * sizeof(uint64_t));

        if(indirectBlockNumber == 0) {
            return;
        }

        // read data from segment buffer if still in cache
        if(indirectBlockNumber > superblock.currentSegment * BLOCKS_PER_SEGMENT) {
            uint64_t cachedBlockNumber = indirectBlockNumber - superblock.currentSegment * BLOCKS_PER_SEGMENT - 1;
            memcpy(buffer, segmentBuffer + cachedBlockNumber * BLOCK_SIZE, BLOCK_SIZE);
        } else {
            device->read(buffer, indirectBlockNumber * sectorsPerBlock, sectorsPerBlock);
        }
    }
}

void Lfs::setBlockInFile(Inode &inode, uint64_t blockNumberInFile, uint8_t* buffer) {
    // determine if block is in direct, indirect or doubly indirect list
    if(blockNumberInFile < 10) {
        // allocate new block, add one because first block is one not zero
        inode.directBlocks[blockNumberInFile] = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment + 1;

        // write data to segment
        memcpy(segmentBuffer + nextBlockInSegment * BLOCK_SIZE, buffer, BLOCK_SIZE);
        // increment next free block
        nextBlockInSegment++;
        // TODO flush if segment full?
    } else if(blockNumberInFile < 10 + BLOCKS_PER_INDIRECT_BLOCK) {
        // allocate 2 new blocks: one for data, one for changed indirect block
        uint64_t newBlockNumber = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment + 1;
        uint64_t newIndirectBlockNumber = newBlockNumber + 1;

        // read indirect block to block buffer
        device->read(blockBuffer, inode.indirectBlocks * sectorsPerBlock, sectorsPerBlock);
        // add block to file
        Util::ByteBuffer::writeU64(blockBuffer, (blockNumberInFile - 10) * sizeof(uint64_t), newBlockNumber);
        
        // write data to segment
        memcpy(segmentBuffer + nextBlockInSegment * BLOCK_SIZE, buffer, BLOCK_SIZE);
        // increment next free block
        nextBlockInSegment++;

        // write changed indirect block
        memcpy(segmentBuffer + nextBlockInSegment * BLOCK_SIZE, blockBuffer, BLOCK_SIZE);
        // increment next free block
        nextBlockInSegment++;

        // TODO flush if segment full?

        // change indirect block to new one
        inode.indirectBlocks = newIndirectBlockNumber;
    } else {

        // allocate 3 new blocks: one for data, one for changed doubly indirect block, one for changed indirect block
        uint64_t newBlockNumber = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment + 1;
        uint64_t newDoublyIndirectBlockNumber = newBlockNumber + 1;
        uint64_t newIndirectBlockNumber = newDoublyIndirectBlockNumber + 1;

         // read doubly indirect block to block buffer
        device->read(blockBuffer, inode.doublyIndirectBlocks * sectorsPerBlock, sectorsPerBlock);
        
        // calculate offsets into indirect blocks
        uint64_t n = blockNumberInFile - 10 - BLOCKS_PER_INDIRECT_BLOCK;
        uint64_t j = n / BLOCKS_PER_INDIRECT_BLOCK;
        uint64_t i = n % BLOCKS_PER_INDIRECT_BLOCK;

        // read old indirect block number
        uint64_t indirectBlockNumber = Util::ByteBuffer::readU64(blockBuffer, j * sizeof(uint64_t));

        // add new indirect block to doubly indirect block
        Util::ByteBuffer::writeU64(blockBuffer, j * sizeof(uint64_t), newIndirectBlockNumber);
        
        // write data to segment
        memcpy(segmentBuffer + nextBlockInSegment * BLOCK_SIZE, buffer, BLOCK_SIZE);
        // increment next free block
        nextBlockInSegment++;

         // write changed doubly indirect block
        memcpy(segmentBuffer + nextBlockInSegment * BLOCK_SIZE, blockBuffer, BLOCK_SIZE);
        // increment next free block
        nextBlockInSegment++;

        // read indirect block to block buffer
        device->read(blockBuffer, indirectBlockNumber * sectorsPerBlock, sectorsPerBlock);

        // add new block to indirect block
        Util::ByteBuffer::writeU64(blockBuffer, i * sizeof(uint64_t), newBlockNumber);

        // write changed indirect block
        memcpy(segmentBuffer + nextBlockInSegment * BLOCK_SIZE, blockBuffer, BLOCK_SIZE);
        // increment next free block
        nextBlockInSegment++;

        // TODO flush if segment full?

        // change doubly indirect block to new one
        inode.doublyIndirectBlocks = newDoublyIndirectBlockNumber;
    }

    // set dirty flag so changes are written next flush
    dirty = true;
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
        getBlockInFile(inode, i, blockBuffer);
        memcpy(buf + i * BLOCK_SIZE, blockBuffer, BLOCK_SIZE);
    }

    // if pos is not block aligned we need to read a partial block
    uint64_t startBlock = pos / BLOCK_SIZE;
    uint64_t startOffset = roundedPosition - pos;

    if(startOffset != 0) {
        getBlockInFile(inode, startBlock, blockBuffer);
        memcpy(buf + startBlock * BLOCK_SIZE, blockBuffer, startOffset);
    }

    // if numBytes is not block aligned we need to read a partial block
    uint64_t endBlock = (pos + numBytes) / BLOCK_SIZE;
    uint64_t endOffset = numBytes - roundedLength;

    if(endOffset != 0) {
        getBlockInFile(inode, endBlock, blockBuffer);
        memcpy(buf + endBlock * BLOCK_SIZE, blockBuffer, endOffset);
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
        setBlockInFile(inode, i, (uint8_t*)buf + i * BLOCK_SIZE);
    }

    // if pos is not block aligned we need to read and write a partial block
    uint64_t startBlock = pos / BLOCK_SIZE;
    uint64_t startOffset = roundedPosition - pos;

    if(startOffset != 0) {
        //clean buffer before use
        memset(blockBuffer, 0, BLOCK_SIZE);

        getBlockInFile(inode, startBlock, blockBuffer);
        memcpy(blockBuffer, buf + startBlock * BLOCK_SIZE, startOffset);
        setBlockInFile(inode, startBlock, blockBuffer);
    }

    // if length is not block aligned we need to read and write a partial block
    uint64_t endBlock = (pos + length) / BLOCK_SIZE;
    uint64_t endOffset = length - roundedLength;

    if(endOffset != 0) {
        //clean buffer before use
        memset(blockBuffer, 0, BLOCK_SIZE);

        getBlockInFile(inode, endBlock, blockBuffer);
        memcpy(blockBuffer, buf + endBlock * BLOCK_SIZE, endOffset);
        setBlockInFile(inode, endBlock, blockBuffer);
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

    Inode inode = {};

    inode.dirty = true;
    inode.fileType = fileType;

    // allocate new inode number
    inodeNumber = nextInodeNumber;
    nextInodeNumber++;

    inodeCache.put(inodeNumber, inode);

    // add directory entry in parent
    uint64_t parentInodeNumber = getParentInodeNumber(path);
    Inode parentInode = getInode(parentInodeNumber);
    Util::ArrayList<DirectoryEntry> parentDirectoryEntries(readDirectoryEntries(parentInodeNumber, parentInode));

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

    inodeCache.remove(inodeNumber);
    inodeMap.remove(inodeNumber);

    // delete directory entry in parent
    uint64_t parentInodeNumber = getParentInodeNumber(path);
    Inode parentInode = getInode(parentInodeNumber);
    Util::ArrayList<DirectoryEntry> oldParentDirectoryEntries(readDirectoryEntries(parentInodeNumber, parentInode));
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

    Util::Array<DirectoryEntry> directoryEntries = readDirectoryEntries(inodeNumber, inode);

    Util::Array<String> res = Util::Array<String>(directoryEntries.length());
    for(size_t i = 0; i < directoryEntries.length(); i++) {
        res[i] = directoryEntries[i].filename;
    }

    return res;
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

        Util::Array<DirectoryEntry> directoryEntries = readDirectoryEntries(currentInodeNumber, currentDir);

        // check if token[i] is in current dir
        bool found = false;
        for(size_t j = 0; j < directoryEntries.length(); j++) {
            if(directoryEntries[j].filename == token[i]) {
                currentInodeNumber = directoryEntries[j].inodeNumber;
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
        device->read(inodeBuffer, entry.inodePosition * sectorsPerBlock, sectorsPerBlock);

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

Util::Array<DirectoryEntry> Lfs::readDirectoryEntries(uint64_t dirInodeNumber, Inode &dirInode) {
    // look for cached entries
    if(directoryEntryCache.containsKey(dirInodeNumber)) {
        return directoryEntryCache.get(dirInodeNumber);
    }

    // read all directory entries form disk
    Util::ArrayList<DirectoryEntry> entries;
    for(size_t i = 0; i < 10 + BLOCKS_PER_INDIRECT_BLOCK + BLOCKS_PER_DOUBLY_INDIRECT_BLOCK; i++) {

        // TODO skip unused entries

        // read block
        getBlockInFile(dirInode, i, blockBuffer);

        // read all entries of block
        for(size_t d = 0; d < BLOCK_SIZE;) {
            DirectoryEntry entry;

            entry.inodeNumber = Util::ByteBuffer::readU64(blockBuffer, d);

            //  inode number 0 signals end of list
            if(entry.inodeNumber == 0) {
                return entries.toArray();
            }

            uint32_t filenameLength = Util::ByteBuffer::readU32(blockBuffer, d + sizeof(entry.inodeNumber));
            entry.filename = Util::ByteBuffer::readString(blockBuffer, d + 12, filenameLength);

            entries.add(entry);

            d += entry.filename.length() + sizeof(filenameLength) + sizeof(entry.inodeNumber);
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
