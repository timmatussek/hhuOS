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
#ifndef __Inode_include__
#define __Inode_include__

#include "lib/string/String.h"

/**
 * Size of inode in bytes
 */
#define INODE_SIZE 105

/**
 * An inode contains metadata and data blocks of a file or directory.
 */
struct Inode {
    /**
     * True if the in-memory inode changed and needs to be written to disk.
     */
    bool dirty;

    /**
     * Size in bytes of the file.
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

#endif