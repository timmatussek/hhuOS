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

    // reset inode map
    inodeMap.clear();

    // reset inode cache
    inodeCache.clear();

    // reset block cache
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
    // TODO
    return Util::Array<String>(0);
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
    // TODO
    return 0;
}

Inode Lfs::getInode(uint64_t inodeNumber)
{
    // TODO
    return Inode{};
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

    for (int i = 0; i < BLOCK_SIZE * superblock.inodeMapSize; i += 20)
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
