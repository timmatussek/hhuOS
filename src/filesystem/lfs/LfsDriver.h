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

#ifndef __LfsDriver_include__
#define __LfsDriver_include__

#include "filesystem/core/FsDriver.h"

extern "C"
{
#include "lib/libc/string.h"
}

/**
 * An implementation of FsDriver for a log-structured file system.
 */
class LfsDriver : public FsDriver
{
private:
    StorageDevice *device = nullptr;

    static const constexpr char *TYPE_NAME = "LfsDriver";

public:
    PROTOTYPE_IMPLEMENT_CLONE(LfsDriver);

    /**
     * Constructor.
     */
    LfsDriver() = default;

    /**
     * Destructor.
     */
    ~LfsDriver() override = default;

    /**
     * Overriding virtual function from FsDriver.
     */
    String getTypeName() override;

    /**
     * Overriding virtual function from FsDriver.
     */
    bool createFs(StorageDevice *device) override;

    /**
     * Overriding virtual function from FsDriver.
     */
    bool mount(StorageDevice *device) override;

    /**
     * Overriding virtual function from FsDriver.
     */
    Util::SmartPointer<FsNode> getNode(const String &path) override;

    /**
     * Overriding virtual function from FsDriver.
     */
    bool createNode(const String &path, uint8_t fileType) override;

    /**
     * Overriding virtual function from FsDriver.
     */
    bool deleteNode(const String &path) override;
};

#endif
