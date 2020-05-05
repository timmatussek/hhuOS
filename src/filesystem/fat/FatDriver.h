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

#ifndef HHUOS_FATDRIVER_H
#define HHUOS_FATDRIVER_H

#include <filesystem/core/FsDriver.h>
#include "FileAllocationTable.h"

class FatDriver : public FsDriver {

private:

    struct MediaInfo {
        FileAllocationTable::MediaDescriptor type;
        uint8_t driveNumber;
        uint16_t sectorsPerTrack;
        uint16_t headCount;
    };

public:

    PROTOTYPE_IMPLEMENT_CLONE(FatDriver);

    /**
     * Constructor.
     */
    FatDriver() = default;

    /**
     * Destructor.
     */
    ~FatDriver() override = default;

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

private:

    static MediaInfo getMediaInfo(StorageDevice &device);
    static FileAllocationTable::BiosParameterBlock createBiosParameterBlock(StorageDevice &device, FileAllocationTable::Type fatType);
    static FileAllocationTable::ExtendedBiosParameterBlock createExtendedBiosParameterBlock(StorageDevice &device, FileAllocationTable::Type fatType);

    static const constexpr char *TYPE_NAME = "FatDriver";
    static const constexpr char *DEFAULT_VOLUME_LABEL = "NO NAME    ";
    static const constexpr char *FAT12_TYPE = "FAT12   ";
    static const constexpr char *FAT16_TYPE = "FAT16   ";
    static const constexpr char *FAT32_TYPE = "FAT32   ";

    static const constexpr uint16_t DEFAULT_ROOT_ENTRIES = 512;
    static const constexpr uint16_t DEFAULT_ROOT_ENTRIES_FLOPPY = 224;
    static const constexpr uint8_t DEFAULT_SIGNATURE = 0x29;
    static const constexpr uint16_t PARTITION_SIGNATURE = 0xaa55;

    static const constexpr uint8_t MAX_SECTORS_PER_CLUSTER = 128;
};

#endif
