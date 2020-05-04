#include <BuildConfig.h>
#include <lib/math/Random.h>
#include <kernel/log/Logger.h>
#include "FatDriver.h"

extern "C" {
#include <lib/libc/math.h>
}

String FatDriver::getTypeName() {
    return TYPE_NAME;
}

bool FatDriver::mount(StorageDevice *device) {
    return false;
}

bool FatDriver::createFs(StorageDevice *device) {
    Util::SmartPointer<uint8_t> bootSector(new uint8_t[device->getSectorSize()]);

    if(!device->read(bootSector.get(), 0, 1)) {
        return false;
    }

    BiosParameterBlock parameterBlock = createBiosParameterBlock(*device, FAT12);
    ExtendedBiosParameterBlock bootRecord = createExtendedBiosParameterBlock(*device, FAT12);

    memset(bootSector.get(), 0, device->getSectorSize());
    memcpy(bootSector.get(), &parameterBlock, sizeof(BiosParameterBlock));
    memcpy(bootSector.get() + sizeof(BiosParameterBlock), &bootRecord, sizeof(ExtendedBiosParameterBlock));

    *reinterpret_cast<uint16_t*>(&bootSector.get()[510]) = PARTITION_SIGNATURE;

    return device->write(bootSector.get(), 0, 1);
}

Util::SmartPointer<FsNode> FatDriver::getNode(const String &path) {
    return Util::SmartPointer<FsNode>();
}

bool FatDriver::createNode(const String &path, uint8_t fileType) {
    return false;
}

bool FatDriver::deleteNode(const String &path) {
    return false;
}

FatDriver::BiosParameterBlock FatDriver::createBiosParameterBlock(StorageDevice &device, FatType fatType) {
    String osVersion = BuildConfig::getVersion();
    while(osVersion.length() > 0 && String::isAlpha(osVersion[0])) {
        osVersion = osVersion.substring(1, osVersion.length());
    }

    String oemName = "hhuOS" + osVersion;

    MediaInfo info = getMediaInfo(device);

    BiosParameterBlock parameterBlock{};

    // Put asm code for endless loop at the beginning of the MBR
    parameterBlock.jmpCode[0] = 0xeb;
    parameterBlock.jmpCode[1] = 0xfe;
    parameterBlock.jmpCode[2] = 0x90;

    memset(parameterBlock.oemName, ' ', sizeof(parameterBlock.oemName));
    memcpy(parameterBlock.oemName, static_cast<char*>(oemName),
           oemName.length() > sizeof(parameterBlock.oemName) ? sizeof(parameterBlock.oemName) : oemName.length());

    parameterBlock.bytesPerSector = device.getSectorSize();

    uint8_t bitsPerAddress;
    switch(fatType) {
        case FAT12:
            bitsPerAddress = 12;
            break;
        case FAT16:
            bitsPerAddress = 16;
            break;
        case FAT32:
            bitsPerAddress = 28;
            break;
    }

    uint32_t neededSectorsPerCluster = device.getSectorCount() / pow(2, bitsPerAddress);

    if(neededSectorsPerCluster < 1) {
        neededSectorsPerCluster = 1;
    } else if(neededSectorsPerCluster > MAX_SECTORS_PER_CLUSTER) {
        neededSectorsPerCluster = MAX_SECTORS_PER_CLUSTER;
    }  else {
        uint32_t x = neededSectorsPerCluster - 1;
        neededSectorsPerCluster = 2;

        while(x >>= 1) {
            neededSectorsPerCluster <<= 1;
        }
    }

    parameterBlock.sectorsPerCluster = neededSectorsPerCluster;

    parameterBlock.reservedSectorCount = 1;
    parameterBlock.fatCount = 2;
    parameterBlock.rootEntryCount = device.getSectorCount() > 5760 ? DEFAULT_ROOT_ENTRIES : DEFAULT_ROOT_ENTRIES_FLOPPY;

    parameterBlock.sectorCount16 = device.getSectorCount() > 65535 ? 0 : device.getSectorCount();

    parameterBlock.mediaDescriptor = info.type;

    switch(fatType) {
        case FAT12:
            bitsPerAddress = 12;
            break;
        case FAT16:
            bitsPerAddress = 16;
            break;
        case FAT32:
            bitsPerAddress = 32;
            break;
    }

    parameterBlock.sectorsPerFat = ((static_cast<uint32_t>(device.getSectorCount()) / parameterBlock.sectorsPerCluster) * bitsPerAddress) / 8 / device.getSectorSize() + 1;

    parameterBlock.sectorsPerTrack = info.sectorsPerTrack;
    parameterBlock.headCount = info.headCount;
    parameterBlock.hiddenSectorCount = 0;

    parameterBlock.sectorCount32 = device.getSectorCount() > 65535 ? device.getSectorCount() : 0;

    return parameterBlock;
}

FatDriver::ExtendedBiosParameterBlock FatDriver::createExtendedBiosParameterBlock(StorageDevice &device, FatType fatType) {
    ExtendedBiosParameterBlock bootRecord{};
    MediaInfo info = getMediaInfo(device);
    Random random;

    bootRecord.driveNumber = info.driveNumber;
    bootRecord.bootSignature = DEFAULT_SIGNATURE;

    bootRecord.volumeId = random.rand(0xff) | (random.rand(0xff) << 8) | (random.rand(0xff) << 16) | (random.rand(0xff) << 24);

    memset(bootRecord.volumeLabel, ' ', sizeof(bootRecord.volumeLabel));
    memcpy(bootRecord.volumeLabel, DEFAULT_VOLUME_LABEL, 11);

    memset(bootRecord.fatType, ' ', sizeof(bootRecord.fatType));

    switch(fatType) {
        case FAT12:
            memcpy(bootRecord.fatType, FAT12_TYPE, 8);
            break;
        case FAT16:
            memcpy(bootRecord.fatType, FAT16_TYPE, 8);
            break;
        case FAT32:
            memcpy(bootRecord.fatType, FAT32_TYPE, 8);
            break;
    }

    return bootRecord;
}

FatDriver::MediaInfo FatDriver::getMediaInfo(StorageDevice &device) {
    MediaInfo info{};

    switch(device.getSectorCount()) {
        case 5760:
            info.type = FLOPPY_35_1440K;
            info.driveNumber = 0x00;
            info.sectorsPerTrack = 36;
            info.headCount = 2;
            break;
        case 2880:
            info.type = FLOPPY_35_1440K;
            info.driveNumber = 0x00;
            info.sectorsPerTrack = 18;
            info.headCount = 2;
            break;
        case 1440:
            info.type = FLOPPY_35_720K;
            info.driveNumber = 0x00;
            info.sectorsPerTrack = 9;
            info.headCount = 2;
            break;
        case 2400:
            info.type = FLOPPY_35_720K;
            info.driveNumber = 0x00;
            info.sectorsPerTrack = 15;
            info.headCount = 2;
            break;
        case 360:
            info.type = FLOPPY_525_180K;
            info.driveNumber = 0x00;
            info.sectorsPerTrack = 9;
            info.headCount = 1;
            break;
        case 720:
            info.type = FLOPPY_525_360K;
            info.driveNumber = 0x00;
            info.sectorsPerTrack = 9;
            info.headCount = 2;
            break;
        case 320:
            info.type = FLOPPY_525_160K;
            info.driveNumber = 0x00;
            info.sectorsPerTrack = 8;
            info.headCount = 1;
            break;
        case 640:
            info.type = FLOPPY_525_320K;
            info.driveNumber = 0x00;
            info.sectorsPerTrack = 8;
            info.headCount = 2;
            break;
        default:
            info.type = FIXED_DISK;
            info.driveNumber = 0x80;
            info.sectorsPerTrack = 0;
            info.headCount = 0;
            break;
    }

    return info;
}
