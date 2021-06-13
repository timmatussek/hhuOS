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

String LfsDriver::getTypeName() {
    return TYPE_NAME;
}

bool LfsDriver::createFs(StorageDevice *device) {
    // use a temporary lfs to format a disk
    Util::SmartPointer<Lfs> tmpLfs = Util::SmartPointer<Lfs>(new Lfs(*device));
    tmpLfs->flush();
    return true;
}

bool LfsDriver::mount(StorageDevice *device) {
    this->lfs = Util::SmartPointer<Lfs>(new Lfs(*device, true));
    return true;
}

Util::SmartPointer<FsNode> LfsDriver::getNode(const String &path) {
    // return nullptr if file does not exist
    if(this->lfs->getFileType(path) == 0) {
        return Util::SmartPointer<FsNode>(nullptr);
    }

    return Util::SmartPointer<FsNode>(new LfsNode(*this->lfs, path));
}

bool LfsDriver::createNode(const String &path, uint8_t fileType) {
    return this->lfs->createNode(path, fileType);
}

bool LfsDriver::deleteNode(const String &path) {
    return this->lfs->deleteNode(path);
}
