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

#ifndef __LfsNode_include__
#define __LfsNode_include__

#include "filesystem/core/Filesystem.h"
#include "filesystem/core/FsNode.h"
#include "Lfs.h"

class LfsNode : public FsNode
{
private:
    Lfs *lfs = nullptr;
    String path;

public:
    /**
     * Constructor.
     */
    LfsNode(Lfs *lfs, const String &path) : lfs(lfs), path(path) {}

    /**
     * Destructor.
     */
    ~LfsNode() override = default;

    /**
     * Overriding virtual function from FsNode.
     */
    String getName() override;

    /**
     * Overriding virtual function from FsNode.
     */
    uint8_t getFileType() override;

    /**
     * Overriding virtual function from FsNode.
     */
    uint64_t getLength() override;

    /**
     * Overriding virtual function from FsNode.
     */
    Util::Array<String> getChildren() override;

    /**
     * Overriding virtual function from FsNode.
     */
    uint64_t readData(char *buf, uint64_t pos, uint64_t numBytes) override;

    /**
     * Overriding virtual function from FsNode.
     */
    uint64_t writeData(char *buf, uint64_t pos, uint64_t length) override;
};

#endif