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
#include "LfsFlushCallback.h"
#include "kernel/thread/Scheduler.h"

Lfs::Lfs(StorageDevice &device, bool mount) : device(device) {

    // sector size is only known at runtime
    sectorsPerBlock = BLOCK_SIZE / device.getSectorSize();

    if(mount) {
        // try to read lfs from disk

        // first block contains information about filesystem
        device.read(blockBuffer.begin(), 0, sectorsPerBlock);

        // do nothing if device does not contain lfs signature
        const uint32_t magic = Util::ByteBuffer::readU32(blockBuffer.begin(), 0);
        if (magic != LFS_MAGIC) {
            return;
        }

        superblock.magic = magic;
        superblock.inodeMapPosition = Util::ByteBuffer::readU64(blockBuffer.begin(), 4);
        superblock.inodeMapSize = Util::ByteBuffer::readU64(blockBuffer.begin(), 12);
        superblock.currentSegment = Util::ByteBuffer::readU64(blockBuffer.begin(), 20);

        // reset caches and load inode map from disk
        inodeCache.clear();
        inodeMap.clear();

        // read whole inode map from disk
        Util::Array<uint8_t> inodeMapBuffer = Util::Array<uint8_t>(BLOCK_SIZE * superblock.inodeMapSize);
        device.read(inodeMapBuffer.begin(), superblock.inodeMapPosition * sectorsPerBlock, superblock.inodeMapSize * sectorsPerBlock);

        nextInodeNumber = 0;

        for (size_t i = 0; i < BLOCK_SIZE * superblock.inodeMapSize; i += 20) {
            uint64_t inodeNum = Util::ByteBuffer::readU64(inodeMapBuffer.begin(), i);

            if (inodeNum == 0) {
                break;
            }

            const InodeMapEntry entry {
                .inodePosition = Util::ByteBuffer::readU64(inodeMapBuffer.begin(), i + 8),
                .inodeOffset = Util::ByteBuffer::readU32(inodeMapBuffer.begin(), i + 16)
            };

            inodeMap.put(inodeNum, entry);

            // keep track of highest in-use inode number
            if(inodeNum >= nextInodeNumber) {
                nextInodeNumber = inodeNum + 1;
            }
        }

        // as we are now in the state the disk is in
        // we do not need to flush until changes appear
        dirty = false;
    } else {
        // initialize empty lfs 

        // initialize super block
        superblock.magic = LFS_MAGIC;
        superblock.inodeMapPosition = 0;
        superblock.inodeMapSize = 0;
        superblock.currentSegment = 0;

        // add empty root dir; root dir is always inode number 1
        const Inode rootDir {
            .dirty = true,
            .size = 0,
            .fileType = FsNode::FileType::DIRECTORY_FILE,
            .directBlocks = {0},
            .indirectBlocks = 0,
            .doublyIndirectBlocks = 0,
        };
        inodeCache.put(1, rootDir);

        // as root dir is inode 1, next free inode is 2
        nextInodeNumber = 2;

        dirty = true;

        // add . and .. (both pointing to / itself)
        addDirectoryEntry(1, ".", 1);
        addDirectoryEntry(1, "..", 1);
    }

    // start a thread to flush at regular intervals
    flushCallback = Util::SmartPointer<LfsFlushCallback>(new LfsFlushCallback(*this));
    flushCallback->start();
}

Lfs::~Lfs() {
    // stop thread
    Kernel::Scheduler::getInstance().kill(*flushCallback);

    // write all cached changes to disk
    flush();
}

void Lfs::flush() {
    // do nothing if no changes occurred since last flush
    if(!dirty) {
        return;
    }

    // clear garbage out of buffer
    memset(blockBuffer.begin(), 0, BLOCK_SIZE);

    // write all dirty inodes
    Util::Array<uint64_t> inodeNumbers = inodeCache.keySet();

    // offset of next inode in current block
    uint32_t inodeOffset = 0;

    for(size_t i = 0; i < inodeNumbers.length(); i++) {
        const uint64_t n = inodeNumbers[i];
        Inode inode = inodeCache.get(n);
        if(inode.dirty) {
            // write block if inode does not fit
            if(BLOCK_SIZE - inodeOffset < INODE_SIZE) {

                // write out segment if full
                if(nextBlockInSegment * BLOCK_SIZE >= SEGMENT_SIZE) {
                    flushSegmentBuffer();
                }

                writeBlockToSegmentBuffer(blockBuffer.begin());

                // clear garbage out of buffer
                memset(blockBuffer.begin(), 0, BLOCK_SIZE);

                inodeOffset = 0;
            }

            // write inode
            Util::ByteBuffer::writeU64(blockBuffer.begin(), inodeOffset + 0, inode.size);
            Util::ByteBuffer::writeU8(blockBuffer.begin(), inodeOffset + 8, inode.fileType);
            Util::ByteBuffer::writeU64(blockBuffer.begin(), inodeOffset + 9, inode.directBlocks[0]);
            Util::ByteBuffer::writeU64(blockBuffer.begin(), inodeOffset + 17, inode.directBlocks[1]);
            Util::ByteBuffer::writeU64(blockBuffer.begin(), inodeOffset + 25, inode.directBlocks[2]);
            Util::ByteBuffer::writeU64(blockBuffer.begin(), inodeOffset + 33, inode.directBlocks[3]);
            Util::ByteBuffer::writeU64(blockBuffer.begin(), inodeOffset + 41, inode.directBlocks[4]);
            Util::ByteBuffer::writeU64(blockBuffer.begin(), inodeOffset + 49, inode.directBlocks[5]);
            Util::ByteBuffer::writeU64(blockBuffer.begin(), inodeOffset + 57, inode.directBlocks[6]);
            Util::ByteBuffer::writeU64(blockBuffer.begin(), inodeOffset + 65, inode.directBlocks[7]);
            Util::ByteBuffer::writeU64(blockBuffer.begin(), inodeOffset + 73, inode.directBlocks[8]);
            Util::ByteBuffer::writeU64(blockBuffer.begin(), inodeOffset + 81, inode.directBlocks[9]);
            Util::ByteBuffer::writeU64(blockBuffer.begin(), inodeOffset + 89, inode.indirectBlocks);
            Util::ByteBuffer::writeU64(blockBuffer.begin(), inodeOffset + 97, inode.doublyIndirectBlocks);

            // update inode map with new inode position
            const InodeMapEntry entry {
                // current block number
                .inodePosition = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment + 1,
                .inodeOffset = inodeOffset
            };

            inodeMap.put(n, entry);

            inodeOffset += INODE_SIZE;

            // remove dirty flag from inode
            inode.dirty = false;
            inodeCache.put(n, inode);
        }
    }

    // write partial block
    // write out segment if full
    if(nextBlockInSegment * BLOCK_SIZE >= SEGMENT_SIZE) {
        flushSegmentBuffer();
    }

    writeBlockToSegmentBuffer(blockBuffer.begin());

    // clear garbage out of buffer
    memset(blockBuffer.begin(), 0, BLOCK_SIZE);

    // write inode map

    // current block number
    superblock.inodeMapPosition = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment + 1;

    // offset of next inode map entry, start at next free block in segment
    uint32_t inodeMapEntryOffset = nextBlockInSegment * BLOCK_SIZE;

    inodeNumbers = inodeMap.keySet();
    for(size_t i = 0; i < inodeNumbers.length(); i++) {
        const uint64_t n = inodeNumbers[i];
        const InodeMapEntry entry = inodeMap.get(n);

        // write out segment if full
        if(inodeMapEntryOffset + INODE_MAP_ENTRY_SIZE >= SEGMENT_SIZE) {
            flushSegmentBuffer();
            inodeMapEntryOffset = 0;
        }

        // write inode map entry
        Util::ByteBuffer::writeU64(segmentBuffer.begin(), inodeMapEntryOffset, n);
        Util::ByteBuffer::writeU64(segmentBuffer.begin(), inodeMapEntryOffset + 8, entry.inodePosition);
        Util::ByteBuffer::writeU32(segmentBuffer.begin(), inodeMapEntryOffset + 16, entry.inodeOffset);

        inodeMapEntryOffset += INODE_MAP_ENTRY_SIZE;
    }

    // write out segment if full
    if(inodeMapEntryOffset + INODE_MAP_ENTRY_SIZE >= SEGMENT_SIZE) {
        flushSegmentBuffer();
        inodeMapEntryOffset = 0;
    }

    // write empty inode map entry to signal end of list
    Util::ByteBuffer::writeU64(segmentBuffer.begin(), inodeMapEntryOffset, 0);
    Util::ByteBuffer::writeU64(segmentBuffer.begin(), inodeMapEntryOffset + 8, 0);
    Util::ByteBuffer::writeU32(segmentBuffer.begin(), inodeMapEntryOffset + 16, 0);

    // write partial segment
    flushSegmentBuffer();

    // calculate inodeMapSize in blocks
    superblock.inodeMapSize = roundUpBlockAddress(inodeNumbers.length() * INODE_MAP_ENTRY_SIZE) / BLOCK_SIZE;

    // write superblock
    Util::ByteBuffer::writeU32(blockBuffer.begin(), 0, superblock.magic);
    Util::ByteBuffer::writeU64(blockBuffer.begin(), 4, superblock.inodeMapPosition);
    Util::ByteBuffer::writeU64(blockBuffer.begin(), 12, superblock.inodeMapSize);
    Util::ByteBuffer::writeU64(blockBuffer.begin(), 20, superblock.currentSegment); 

    device.write(blockBuffer.begin(), 0, sectorsPerBlock);

    // clear garbage out of buffer
    memset(blockBuffer.begin(), 0, BLOCK_SIZE);

    // reset segment buffer
    nextBlockInSegment = 0;

    // reset dirty flag
    dirty = false;
}

void Lfs::getBlockInFile(const Inode &inode, uint64_t blockNumberInFile, uint8_t* buffer) {
    // determine if block is in direct, indirect or doubly indirect list
    if(blockNumberInFile < 10) {
        // find blocknumber and read block
        const uint64_t blockNumber = inode.directBlocks[blockNumberInFile];

        if(blockNumber == 0) {
            return;
        }

        // read data from segment buffer if still in cache
        if(blockNumber > superblock.currentSegment * BLOCKS_PER_SEGMENT) {
            const uint64_t cachedBlockNumber = blockNumber - superblock.currentSegment * BLOCKS_PER_SEGMENT - 1;
            memcpy(buffer, segmentBuffer.begin() + cachedBlockNumber * BLOCK_SIZE, BLOCK_SIZE);
        } else {
            device.read(buffer, blockNumber * sectorsPerBlock, sectorsPerBlock);
        }
    } else if(blockNumberInFile < 10 + BLOCKS_PER_INDIRECT_BLOCK) {

        if(inode.indirectBlocks == 0) {
            return;
        }

        // read indirect block to block buffer
        // read data from segment buffer if still in cache
        if(inode.indirectBlocks > superblock.currentSegment * BLOCKS_PER_SEGMENT) {
            const uint64_t cachedBlockNumber = inode.indirectBlocks - superblock.currentSegment * BLOCKS_PER_SEGMENT - 1;
            memcpy(blockBuffer.begin(), segmentBuffer.begin() + cachedBlockNumber * BLOCK_SIZE, BLOCK_SIZE);
        } else {
            device.read(blockBuffer.begin(), inode.indirectBlocks * sectorsPerBlock, sectorsPerBlock);
        }

        // find blocknumber and read block
        const uint64_t indirectBlockNumber = Util::ByteBuffer::readU64(blockBuffer.begin(), (blockNumberInFile - 10) * sizeof(uint64_t));

        if(indirectBlockNumber == 0) {
            return;
        }

        // read data from segment buffer if still in cache
        if(indirectBlockNumber > superblock.currentSegment * BLOCKS_PER_SEGMENT) {
            const uint64_t cachedBlockNumber = indirectBlockNumber - superblock.currentSegment * BLOCKS_PER_SEGMENT - 1;
            memcpy(buffer, segmentBuffer.begin() + cachedBlockNumber * BLOCK_SIZE, BLOCK_SIZE);
        } else {
            device.read(buffer, indirectBlockNumber * sectorsPerBlock, sectorsPerBlock);
        }
    } else {

        if(inode.doublyIndirectBlocks == 0) {
            return;
        }

        // read doubly indirect block to block buffer
        // read data from segment buffer if still in cache
        if(inode.doublyIndirectBlocks > superblock.currentSegment * BLOCKS_PER_SEGMENT) {
            const uint64_t cachedBlockNumber = inode.doublyIndirectBlocks - superblock.currentSegment * BLOCKS_PER_SEGMENT - 1;
            memcpy(blockBuffer.begin(), segmentBuffer.begin() + cachedBlockNumber * BLOCK_SIZE, BLOCK_SIZE);
        } else {
            device.read(blockBuffer.begin(), inode.doublyIndirectBlocks * sectorsPerBlock, sectorsPerBlock);
        }
        
        // calculate offsets into indirect blocks
        const uint64_t n = blockNumberInFile - 10 - BLOCKS_PER_INDIRECT_BLOCK;
        const uint64_t j = n / BLOCKS_PER_INDIRECT_BLOCK;
        const uint64_t i = n % BLOCKS_PER_INDIRECT_BLOCK;

        // find blocknumber and read indirect block to buffer
        const uint64_t doublyIndirectBlockNumber = Util::ByteBuffer::readU64(blockBuffer.begin(), j * sizeof(uint64_t));

        if(doublyIndirectBlockNumber == 0) {
            return;
        }

        // read data from segment buffer if still in cache
        if(doublyIndirectBlockNumber > superblock.currentSegment * BLOCKS_PER_SEGMENT) {
            const uint64_t cachedBlockNumber = doublyIndirectBlockNumber - superblock.currentSegment * BLOCKS_PER_SEGMENT - 1;
            memcpy(blockBuffer.begin(), segmentBuffer.begin() + cachedBlockNumber * BLOCK_SIZE, BLOCK_SIZE);
        } else {
            device.read(blockBuffer.begin(), doublyIndirectBlockNumber * sectorsPerBlock, sectorsPerBlock);
        }

        // find blocknumber and read block
        const uint64_t indirectBlockNumber = Util::ByteBuffer::readU64(blockBuffer.begin(), i * sizeof(uint64_t));

        if(indirectBlockNumber == 0) {
            return;
        }

        // read data from segment buffer if still in cache
        if(indirectBlockNumber > superblock.currentSegment * BLOCKS_PER_SEGMENT) {
            const uint64_t cachedBlockNumber = indirectBlockNumber - superblock.currentSegment * BLOCKS_PER_SEGMENT - 1;
            memcpy(buffer, segmentBuffer.begin() + cachedBlockNumber * BLOCK_SIZE, BLOCK_SIZE);
        } else {
            device.read(buffer, indirectBlockNumber * sectorsPerBlock, sectorsPerBlock);
        }
    }
}

void Lfs::setBlockInFile(Inode &inode, uint64_t blockNumberInFile, const uint8_t* buffer) {
    // determine if block is in direct, indirect or doubly indirect list
    if(blockNumberInFile < 10) {
        // allocate new block, add one because first block is one not zero
        inode.directBlocks[blockNumberInFile] = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment + 1;

        writeBlockToSegmentBuffer(buffer);

        // flush if segment full
        if(nextBlockInSegment >= BLOCKS_PER_SEGMENT) {
            flush();
        }

    } else if(blockNumberInFile < 10 + BLOCKS_PER_INDIRECT_BLOCK) {
        // allocate 2 new blocks: one for data, one for changed indirect block
        const uint64_t newBlockNumber = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment + 1;
        const uint64_t newIndirectBlockNumber = newBlockNumber + 1;

        // read indirect block to block buffer
        device.read(blockBuffer.begin(), inode.indirectBlocks * sectorsPerBlock, sectorsPerBlock);
        // add block to file
        Util::ByteBuffer::writeU64(blockBuffer.begin(), (blockNumberInFile - 10) * sizeof(uint64_t), newBlockNumber);
        
        // write data block
        writeBlockToSegmentBuffer(buffer);

        // flush if segment full
        if(nextBlockInSegment >= BLOCKS_PER_SEGMENT) {
            flush();
        }

        // write changed indirect block
        writeBlockToSegmentBuffer(blockBuffer.begin());

        // flush if segment full
        if(nextBlockInSegment >= BLOCKS_PER_SEGMENT) {
            flush();
        }

        // change indirect block to new one
        inode.indirectBlocks = newIndirectBlockNumber;
    } else {
        // allocate 3 new blocks: one for data, one for changed doubly indirect block, one for changed indirect block
        const uint64_t newBlockNumber = superblock.currentSegment * BLOCKS_PER_SEGMENT + nextBlockInSegment + 1;
        const uint64_t newDoublyIndirectBlockNumber = newBlockNumber + 1;
        const uint64_t newIndirectBlockNumber = newDoublyIndirectBlockNumber + 1;

         // read doubly indirect block to block buffer
        device.read(blockBuffer.begin(), inode.doublyIndirectBlocks * sectorsPerBlock, sectorsPerBlock);
        
        // calculate offsets into indirect blocks
        const uint64_t n = blockNumberInFile - 10 - BLOCKS_PER_INDIRECT_BLOCK;
        const uint64_t j = n / BLOCKS_PER_INDIRECT_BLOCK;
        const uint64_t i = n % BLOCKS_PER_INDIRECT_BLOCK;

        // read old indirect block number
        const uint64_t indirectBlockNumber = Util::ByteBuffer::readU64(blockBuffer.begin(), j * sizeof(uint64_t));

        // add new indirect block to doubly indirect block
        Util::ByteBuffer::writeU64(blockBuffer.begin(), j * sizeof(uint64_t), newIndirectBlockNumber);
        
        // write data block
        writeBlockToSegmentBuffer(buffer);

        // flush if segment full
        if(nextBlockInSegment >= BLOCKS_PER_SEGMENT) {
            flush();
        }

        // write changed doubly indirect block
        writeBlockToSegmentBuffer(blockBuffer.begin());

        // flush if segment full
        if(nextBlockInSegment >= BLOCKS_PER_SEGMENT) {
            flush();
        }

        // read indirect block to block buffer
        device.read(blockBuffer.begin(), indirectBlockNumber * sectorsPerBlock, sectorsPerBlock);

        // add new block to indirect block
        Util::ByteBuffer::writeU64(blockBuffer.begin(), i * sizeof(uint64_t), newBlockNumber);

        // write changed indirect block
        writeBlockToSegmentBuffer(blockBuffer.begin());

        // flush if segment full
        if(nextBlockInSegment >= BLOCKS_PER_SEGMENT) {
            flush();
        }

        // change doubly indirect block to new one
        inode.doublyIndirectBlocks = newDoublyIndirectBlockNumber;
    }

    // set dirty flag so changes are written next flush
    dirty = true;
}

uint64_t Lfs::readData(const String &path, char *buf, uint64_t pos, uint64_t numBytes) {
    const uint64_t inodeNumber = getInodeNumber(path);

    if (inodeNumber == 0) {
        return 0;
    }

    const Inode inode = getInode(inodeNumber);

    const uint64_t roundedPosition = roundUpBlockAddress(pos);
    const uint64_t roundedLength  = roundDownBlockAddress(numBytes);

    const uint64_t start = roundedPosition / BLOCK_SIZE;
    const uint64_t end = (roundedPosition + roundedLength) / BLOCK_SIZE;

    for(size_t i = start; i < end; i++) {
        getBlockInFile(inode, i, blockBuffer.begin());
        memcpy(buf + i * BLOCK_SIZE, blockBuffer.begin(), BLOCK_SIZE);
    }

    // if pos is not block aligned we need to read a partial block
    const uint64_t startBlock = pos / BLOCK_SIZE;
    const uint64_t startOffset = roundedPosition - pos;

    if(startOffset != 0) {
        getBlockInFile(inode, startBlock, blockBuffer.begin());
        memcpy(buf + startBlock * BLOCK_SIZE, blockBuffer.begin(), startOffset);
    }

    // if numBytes is not block aligned we need to read a partial block
    const uint64_t endBlock = (pos + numBytes) / BLOCK_SIZE;
    const uint64_t endOffset = numBytes - roundedLength;

    if(endOffset != 0) {
        getBlockInFile(inode, endBlock, blockBuffer.begin());
        memcpy(buf + endBlock * BLOCK_SIZE, blockBuffer.begin(), endOffset);
    }

    return numBytes;
}

uint64_t Lfs::writeData(const String &path, char *buf, uint64_t pos, uint64_t length) {
    const uint64_t inodeNumber = getInodeNumber(path);

    if (inodeNumber == 0) {
        return 0;
    }

    Inode inode = getInode(inodeNumber);

    // updated inode needs to be rewritten to disk
    inode.dirty = true;

    const uint64_t roundedPosition = roundUpBlockAddress(pos);
    const uint64_t roundedLength  = roundDownBlockAddress(length);

    const uint64_t start = roundedPosition / BLOCK_SIZE;
    const uint64_t end = (roundedPosition + roundedLength) / BLOCK_SIZE;

    for(size_t i = start; i < end; i++) {
        setBlockInFile(inode, i, (uint8_t*)buf + i * BLOCK_SIZE);
    }

    // if pos is not block aligned we need to read and write a partial block
    const uint64_t startBlock = pos / BLOCK_SIZE;
    const uint64_t startOffset = roundedPosition - pos;

    if(startOffset != 0) {
        //clean buffer before use
        memset(blockBuffer.begin(), 0, BLOCK_SIZE);

        getBlockInFile(inode, startBlock, blockBuffer.begin());
        memcpy(blockBuffer.begin(), buf + startBlock * BLOCK_SIZE, startOffset);
        setBlockInFile(inode, startBlock, blockBuffer.begin());
    }

    // if length is not block aligned we need to read and write a partial block
    const uint64_t endBlock = (pos + length) / BLOCK_SIZE;
    const uint64_t endOffset = length - roundedLength;

    if(endOffset != 0) {
        //clean buffer before use
        memset(blockBuffer.begin(), 0, BLOCK_SIZE);

        getBlockInFile(inode, endBlock, blockBuffer.begin());
        memcpy(blockBuffer.begin(), buf + endBlock * BLOCK_SIZE, endOffset);
        setBlockInFile(inode, endBlock, blockBuffer.begin());
    }

    // update file size
    inode.size += length;

    // update inode in cache
    inodeCache.put(inodeNumber, inode);

    return length;
}

bool Lfs::createNode(const String &path, uint8_t fileType) {
    uint64_t inodeNumber = getInodeNumber(path);

    // check if already exists
    if (inodeNumber != 0) {
        return false;
    }

    const Inode inode {
        .dirty = true,
        .fileType = fileType
    };

    // allocate new inode number
    inodeNumber = nextInodeNumber;
    nextInodeNumber++;

    inodeCache.put(inodeNumber, inode);

    // add directory entry in parent
    const uint64_t parentInodeNumber = getParentInodeNumber(path);
    addDirectoryEntry(parentInodeNumber, getFileName(path), inodeNumber);

    // if fileType is directory add . and ..
    if(fileType == FsNode::FileType::DIRECTORY_FILE) {
        addDirectoryEntry(inodeNumber, ".", inodeNumber);
        addDirectoryEntry(inodeNumber, "..", parentInodeNumber);
    }

    return true;
}

bool Lfs::deleteNode(const String &path) {
    const uint64_t inodeNumber = getInodeNumber(path);

    if (inodeNumber == 0) {
        return false;
    }

    // delete directory entry in parent
    const uint64_t parentInodeNumber = getParentInodeNumber(path);
    deleteDirectoryEntry(parentInodeNumber, getFileName(path));

    // delete cached entries
    if(inodeCache.containsKey(inodeNumber)) {
        inodeCache.remove(inodeNumber);
    }
    if(inodeMap.containsKey(inodeNumber)) {
        inodeMap.remove(inodeNumber);
    }

    return true;
}

uint8_t Lfs::getFileType(const String &path) {
    const uint64_t inodeNumber = getInodeNumber(path);

    if (inodeNumber == 0) {
        return 0;
    }

    const Inode inode = getInode(inodeNumber);

    return inode.fileType;
}

uint64_t Lfs::getLength(const String &path) {
    const uint64_t inodeNumber = getInodeNumber(path);

    if (inodeNumber == 0) {
        return 0;
    }

    const Inode inode = getInode(inodeNumber);

    return inode.size;
}

Util::Array<String> Lfs::getChildren(const String &path) {
    const uint64_t inodeNumber = getInodeNumber(path);

    if(inodeNumber == 0) {
        return Util::Array<String>(0);
    }

    const Inode inode = getInode(inodeNumber);

    if(inode.fileType != FsNode::FileType::DIRECTORY_FILE) {
        return Util::Array<String>(0);
    }

    return readDirectoryEntries(inodeNumber);
}

String Lfs::getFileName(const String &path) {
    const Util::Array<String> token = path.split(Filesystem::SEPARATOR);
    return token[token.length() - 1];
}

uint64_t Lfs::getInodeNumber(const String &path) {
    // split the path to search left to right
    const Util::Array<String> token = path.split(Filesystem::SEPARATOR);

    // starting at root directory (always inode 1)
    uint64_t currentInodeNumber = 1;

    for(size_t i = 0; i < token.length(); i++) {
        // find token[i] in currentDir
        currentInodeNumber = findDirectoryEntry(currentInodeNumber, token[i]);

        // check if it could not be found
        if(currentInodeNumber == 0) {
            return 0;
        }
    }

    return currentInodeNumber;
}

uint64_t Lfs::getParentInodeNumber(const String &path) {
    // split the path
    const Util::Array<String> token = path.split(Filesystem::SEPARATOR);

    String parentPath = "/";

    // rebuild path without last element
    for(size_t i = 0; i < token.length() - 1; i++) {
        parentPath += token[i] + "/";
    }

    return getInodeNumber(parentPath);
}


Inode Lfs::getInode(uint64_t inodeNumber) {
    // check if inode is cached
    if(inodeCache.containsKey(inodeNumber)) {
        return inodeCache.get(inodeNumber);
    } else {
        // else load it from disk
        const InodeMapEntry entry = inodeMap.get(inodeNumber);

        // read block that contains inode
        uint8_t inodeBuffer[BLOCK_SIZE];
        device.read(inodeBuffer, entry.inodePosition * sectorsPerBlock, sectorsPerBlock);

        // read inode data at offset
        const Inode inode {
            .size = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 0),
            .fileType = Util::ByteBuffer::readU8(inodeBuffer, entry.inodeOffset + 8),
            .directBlocks = {
                Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 9),
                Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 17),
                Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 25),
                Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 33),
                Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 41),
                Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 49),
                Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 57),
                Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 65),
                Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 73),
                Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 81)
            },
            .indirectBlocks = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 89),
            .doublyIndirectBlocks = Util::ByteBuffer::readU64(inodeBuffer, entry.inodeOffset + 97)
        };

        // add to cache
        inodeCache.put(inodeNumber, inode);

        return inode;
    }
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

void Lfs::addDirectoryEntry(uint64_t dirInodeNumber, const String &name, uint64_t entryInodeNumber) {
    Inode inode = getInode(dirInodeNumber);

    const size_t entrySize = name.length() + 12;

    // allocate a buffer for all directory entries
    // make sure it fits at least all current blocks plus
    // the new entry
    const size_t bufferSize = roundUpBlockAddress(inode.size + entrySize);
    Util::Array<uint8_t> buffer = Util::Array<uint8_t>(bufferSize);
    memset(buffer.begin(), 0, bufferSize);

    // read all blocks
    for(size_t i = 0; i * BLOCK_SIZE < inode.size; i++) {
        getBlockInFile(inode, i, buffer.begin() + i * BLOCK_SIZE);
    }

    // add new entry at end
    Util::ByteBuffer::writeU64(buffer.begin(), inode.size + 0, entryInodeNumber);
    Util::ByteBuffer::writeU32(buffer.begin(), inode.size + 8, name.length());
    Util::ByteBuffer::writeString(buffer.begin(), inode.size + 12, name);

    // write all blocks
    for(size_t i = 0; i * BLOCK_SIZE < bufferSize; i++) {
        setBlockInFile(inode, i, buffer.begin() + i * BLOCK_SIZE);
    }

    // update inode
    inode.dirty = true;
    inode.size += entrySize;
    inodeCache.put(dirInodeNumber, inode);
}

void Lfs::deleteDirectoryEntry(uint64_t dirInodeNumber, const String &name) {
    Inode inode = getInode(dirInodeNumber);

    // allocate a buffer for all directory entries
    // make sure it fits at least all current blocks
    const size_t bufferSize = roundUpBlockAddress(inode.size);
    Util::Array<uint8_t> buffer = Util::Array<uint8_t>(bufferSize);
    memset(buffer.begin(), 0, bufferSize);

    // read all blocks
    for(size_t i = 0; i * BLOCK_SIZE < inode.size; i++) {
        getBlockInFile(inode, i, buffer.begin() + i * BLOCK_SIZE);
    }

    // find offset of entry to delete
    size_t offset = 0;
    size_t entrySize = 0;
    bool found = false;

    // read all entries
    for(size_t d = 0; d < bufferSize;) {
        const uint64_t inodeNumber = Util::ByteBuffer::readU64(buffer.begin(), d);

        //  inode number 0 signals end of list
        if(inodeNumber == 0) {
            break;
        }

        const uint32_t filenameLength = Util::ByteBuffer::readU32(buffer.begin(), d + 8);
        const String filename = Util::ByteBuffer::readString(buffer.begin(), d + 12, filenameLength);

        // if found note its offset
        if(filename == name) {
            offset = d;
            entrySize = filenameLength + 8 + 4;
            found = true;
            break;
        }

        // skip to next entry
        d += filenameLength + 8 + 4;
    }

    // if not found do nothing
    if(!found) {
        return;
    }

    // move all later entries over one spot, thus deleting this entry
    memmove(buffer.begin() + offset, buffer.begin() + offset + entrySize, bufferSize - (offset + entrySize));

    // write all blocks
    for(size_t i = 0; i * BLOCK_SIZE < bufferSize; i++) {
        setBlockInFile(inode, i, buffer.begin() + i * BLOCK_SIZE);
    }

    // update inode
    inode.dirty = true;
    inode.size -= entrySize;
    inodeCache.put(dirInodeNumber, inode);
}

uint64_t Lfs::findDirectoryEntry(uint64_t dirInodeNumber, const String &name) {
    Inode inode = getInode(dirInodeNumber);

    // allocate a buffer for all directory entries
    // make sure it fits at least all current blocks
    const size_t bufferSize = roundUpBlockAddress(inode.size);
    Util::Array<uint8_t> buffer = Util::Array<uint8_t>(bufferSize);
    memset(buffer.begin(), 0, bufferSize);

    // read all blocks
    for(size_t i = 0; i * BLOCK_SIZE < inode.size; i++) {
        getBlockInFile(inode, i, buffer.begin() + i * BLOCK_SIZE);
    }

    // read all entries
    for(size_t d = 0; d < bufferSize;) {
        const uint64_t inodeNumber = Util::ByteBuffer::readU64(buffer.begin(), d);

        //  inode number 0 signals end of list
        if(inodeNumber == 0) {
            break;
        }

        const uint32_t filenameLength = Util::ByteBuffer::readU32(buffer.begin(), d + 8);
        const String filename = Util::ByteBuffer::readString(buffer.begin(), d + 12, filenameLength);

        // if found return its inode number
        if(filename == name) {
            return inodeNumber;
        }

        // skip to next entry
        d += filenameLength + 8 + 4;
    }

    return 0;
}

Util::Array<String> Lfs::readDirectoryEntries(uint64_t dirInodeNumber) {
    Inode inode = getInode(dirInodeNumber);

    // allocate a buffer for all directory entries
    // make sure it fits at least all current blocks
    const size_t bufferSize = roundUpBlockAddress(inode.size);
    Util::Array<uint8_t> buffer = Util::Array<uint8_t>(bufferSize);
    memset(buffer.begin(), 0, bufferSize);

    // read all blocks
    Util::ArrayList<String> entries;
    for(size_t i = 0; i * BLOCK_SIZE < inode.size; i++) {
        getBlockInFile(inode, i, buffer.begin() + i * BLOCK_SIZE);
    }

    // read all entries
    for(size_t d = 0; d < bufferSize;) {
        const uint64_t inodeNumber = Util::ByteBuffer::readU64(buffer.begin(), d);

        //  inode number 0 signals end of list
        if(inodeNumber == 0) {
            break;
        }

        const uint32_t filenameLength = Util::ByteBuffer::readU32(buffer.begin(), d + 8);
        const String filename = Util::ByteBuffer::readString(buffer.begin(), d + 12, filenameLength);

        entries.add(filename);

        // skip to next entry
        d += filenameLength + 8 + 4;
    }

    return entries.toArray();
}

void Lfs::writeBlockToSegmentBuffer(const uint8_t* block) {
    // copy data to segment buffer
    memcpy(segmentBuffer.begin() + nextBlockInSegment * BLOCK_SIZE, block, BLOCK_SIZE);
    // advance to next block
    nextBlockInSegment++;
}

void Lfs::flushSegmentBuffer() {
    // write segment, calculate offset and size of segment, offset is incremented by one block for superblock
    device.write(segmentBuffer.begin(), superblock.currentSegment * BLOCKS_PER_SEGMENT * sectorsPerBlock + 1 * sectorsPerBlock, BLOCKS_PER_SEGMENT * sectorsPerBlock);
    // advance to next segment
    superblock.currentSegment++;
    // reset to first block
    nextBlockInSegment = 0;
}