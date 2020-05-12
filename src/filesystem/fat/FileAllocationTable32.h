/*
 * Copyright (C) 2020 Heinrich-Heine-Universitaet Duesseldorf,
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

#ifndef HHUOS_FILEALLOCATIONTABLE32_H
#define HHUOS_FILEALLOCATIONTABLE32_H

#include "FileAllocationTable.h"
#include <lib/util/SmartPointer.h>

class FileAllocationTable32 : public FileAllocationTable {

public:

    explicit FileAllocationTable32(StorageDevice &device);

    FileAllocationTable32(FileAllocationTable32 &copy) = delete;

    FileAllocationTable32& operator=(FileAllocationTable32 &copy) = delete;

    ~FileAllocationTable32() override = default;

    uint32_t getEntry(uint32_t index) override;

    void setEntry(uint32_t index, uint32_t value) override;

private:

    Util::ArrayList<Util::SmartPointer<uint32_t>> tables;
    
};

#endif
