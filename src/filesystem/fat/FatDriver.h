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

class FatDriver : public FsDriver {

private:

    enum FatType: uint8_t {
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

    struct MediaInfo {
        MediaDescriptor type;
        uint8_t driveNumber;
        uint16_t sectorsPerTrack;
        uint16_t headCount;
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
    static FatDriver::BiosParameterBlock createBiosParameterBlock(StorageDevice &device, FatType fatType);
    static FatDriver::ExtendedBiosParameterBlock createExtendedBiosParameterBlock(StorageDevice &device, FatType fatType);

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
