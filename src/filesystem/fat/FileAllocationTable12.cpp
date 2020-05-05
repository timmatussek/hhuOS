#include "FileAllocationTable12.h"

FileAllocationTable12::FileAllocationTable12(StorageDevice &device) : FileAllocationTable(device) {
    BiosParameterBlock &parameterBlock = getBiosParameterBlock();

    for(uint32_t i = 0; i < getBiosParameterBlock().fatCount; i++) {
        Util::SmartPointer<uint8_t> fat(new uint8_t[parameterBlock.sectorsPerFat * device.getSectorSize()]);
        device.read(fat.get(), parameterBlock.reservedSectorCount + i * parameterBlock.sectorsPerFat, parameterBlock.sectorsPerFat);
        tables.add(fat);
    }
}

uint32_t FileAllocationTable12::getEntry(uint32_t index) {
    uint8_t *fat = tables.get(0).get();
    uint16_t tableIndex = index + index / 2;

    uint16_t fatEntry = fat[tableIndex] | (fat[tableIndex + 1] << 8);
    if(index & 0x0001) {
        fatEntry >>= 4;
    } else {
        fatEntry &= 0x0fff;
    }

    return fatEntry;
}

void FileAllocationTable12::setEntry(uint32_t index, uint32_t value) {
    for(uint32_t i = 0; i < getBiosParameterBlock().fatCount; i++) {
        uint8_t *fat = tables.get(0).get();
        uint16_t tableIndex = index + index / 2;

        if(index & 0x0001) {
            fat[tableIndex] = (fat[tableIndex] & 0x0f) | (value << 4);
            fat[tableIndex + 1] = value >> 4;
        } else {
            fat[tableIndex] = value;
            fat[tableIndex + 1] = (fat[tableIndex + 1] & 0xf0) | ((value >> 8) & 0x0f);
        }

        BiosParameterBlock parameterBlock = getBiosParameterBlock();
        getDevice().write(fat + tableIndex / getDevice().getSectorSize(), parameterBlock.reservedSectorCount + i * parameterBlock.sectorsPerFat + tableIndex / getDevice().getSectorSize(), 1);
    }
}
