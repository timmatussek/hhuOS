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

#include "LfsDriver.h"
#include "LfsNode.h"

String LfsDriver::getTypeName()
{
    return TYPE_NAME;
}

bool LfsDriver::createFs(StorageDevice *device)
{
    return false;
}

bool LfsDriver::mount(StorageDevice *device)
{
    return false;
}

Util::SmartPointer<FsNode> LfsDriver::getNode(const String &path)
{
    return Util::SmartPointer<FsNode>(new LfsNode());
}

bool LfsDriver::createNode(const String &path, uint8_t fileType)
{
    return false;
}

bool LfsDriver::deleteNode(const String &path)
{
    return false;
}
