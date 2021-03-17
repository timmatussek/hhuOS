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
    if (!readLfsFromDevice())
    {
        reset();
    }
}

Lfs::~Lfs()
{
    flush();
}

void Lfs::reset()
{
}

bool Lfs::flush()
{
    return false;
}

uint64_t Lfs::readData(const String &path, char *buf, uint64_t pos, uint64_t numBytes)
{
    return 0;
}

uint64_t Lfs::writeData(const String &path, char *buf, uint64_t pos, uint64_t length)
{
    return 0;
}

bool Lfs::createNode(const String &path, uint8_t fileType)
{
    return false;
}

bool Lfs::deleteNode(const String &path)
{
    return false;
}

uint8_t Lfs::getFileType(const String &path)
{
    return 0;
}

uint64_t Lfs::getLength(const String &path)
{
    return 0;
}

Util::Array<String> Lfs::getChildren(const String &path)
{
    return Util::Array<String>(0);
}

bool Lfs::readLfsFromDevice()
{
    return false;
}
