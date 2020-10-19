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

#ifndef HHUOS_FILEALLOCATIONTABLE_H
#define HHUOS_FILEALLOCATIONTABLE_H

#include <device/storage/device/StorageDevice.h>

class FileAllocationTable {

public:

    enum Type: uint8_t {
        FAT12,
        FAT16,
        FAT32
    };

    enum MediaDescriptor: uint8_t {
        FLOPPY_35_1440K = 0xf0,
        FIXED_DISK = 0xf8,
        FLOPPY_35_720K = 0xf9,
        FLOPPY_525_180K = 0xfc,
        FLOPPY_525_360K = 0xfd,
        FLOPPY_525_160K = 0xfe,
        FLOPPY_525_320K = 0xff,
    };

    struct BiosParameterBlock {
        uint8_t jmpCode[3];
        char oemName[8];
        uint16_t bytesPerSector;
        uint8_t sectorsPerCluster;
        uint16_t reservedSectorCount;
        uint8_t fatCount;
        uint16_t rootEntryCount;
        uint16_t sectorCount16;
        MediaDescriptor mediaDescriptor;
        uint16_t sectorsPerFat;
        uint16_t sectorsPerTrack;
        uint16_t headCount;
        uint32_t hiddenSectorCount;
        uint32_t sectorCount32;
    } __attribute__((packed));

    struct ExtendedBiosParameterBlock {
        uint8_t driveNumber;
        uint8_t reserved1;
        uint8_t bootSignature;
        uint32_t volumeId;
        char volumeLabel[11];
        char fatType[8];
    } __attribute__((packed));

public:

    explicit FileAllocationTable(StorageDevice &device);

    FileAllocationTable(FileAllocationTable &copy) = delete;

    FileAllocationTable& operator=(FileAllocationTable &copy) = delete;

    virtual ~FileAllocationTable() = default;

    virtual uint32_t getEntry(uint32_t index) = 0;

    virtual void setEntry(uint32_t index, uint32_t value) = 0;

    virtual Type getType() = 0;

protected:

    StorageDevice& getDevice();

    BiosParameterBlock& getBiosParameterBlock();

private:

    StorageDevice &device;
    BiosParameterBlock biosParameterBlock{};
};

#endif
