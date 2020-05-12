/*
 * Copyright (C) 2020 Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Michael Schoettner
 * Institute of Computer Science, Department Operating Systems
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#ifndef HHUOS_FILEALLOCATIONTABLE16_H
#define HHUOS_FILEALLOCATIONTABLE16_H

#include "FileAllocationTable.h"
#include <lib/util/SmartPointer.h>

class FileAllocationTable16 : public FileAllocationTable {

public:

    explicit FileAllocationTable16(StorageDevice &device);

    FileAllocationTable16(FileAllocationTable16 &copy) = delete;

    FileAllocationTable16& operator=(FileAllocationTable16 &copy) = delete;

    ~FileAllocationTable16() override = default;

    uint32_t getEntry(uint32_t index) override;

    void setEntry(uint32_t index, uint32_t value) override;

private:

    Util::ArrayList<Util::SmartPointer<uint16_t>> tables;

};

#endif
