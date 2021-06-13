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
#ifndef __Superblock_include__
#define __Superblock_include__

#include "lib/string/String.h"

/**
 * The superblock contains information about the filesystem.
 * It is always at block 0.
 */
struct Superblock {
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

#endif