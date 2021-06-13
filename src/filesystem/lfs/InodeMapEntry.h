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
#ifndef __InodeMapEntry_include__
#define __InodeMapEntry_include__

#include "lib/string/String.h"

/**
 * Size of inode map entry in bytes
 */
#define INODE_MAP_ENTRY_SIZE 20

/**
 * The inode map contains the position of each active inode.
 */
struct InodeMapEntry {
    /**
     * The block the inode is stored in.
     */
    uint64_t inodePosition;

    /**
     * The offset inside the block.
     */
    uint32_t inodeOffset;
};

#endif