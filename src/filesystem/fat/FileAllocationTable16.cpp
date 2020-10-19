#include "FileAllocationTable16.h"

FileAllocationTable16::FileAllocationTable16(StorageDevice &device) : FileAllocationTable(device) {
    BiosParameterBlock &parameterBlock = getBiosParameterBlock();

    for(uint32_t i = 0; i < getBiosParameterBlock().fatCount; i++) {
        Util::SmartPointer<uint16_t> fat(new uint16_t[(parameterBlock.sectorsPerFat * device.getSectorSize()) / 2]);
        device.read(reinterpret_cast<uint8_t *>(fat.get()), parameterBlock.reservedSectorCount + i * parameterBlock.sectorsPerFat, parameterBlock.sectorsPerFat);
        tables.add(fat);
    }
}

uint32_t FileAllocationTable16::getEntry(uint32_t index) {
    return tables.get(0).get()[index];
}

void FileAllocationTable16::setEntry(uint32_t index, uint32_t value) {
    for(uint32_t i = 0; i < getBiosParameterBlock().fatCount; i++) {
        uint16_t *fat = tables.get(0).get();
        tables.get(0).get()[index] = value;

        BiosParameterBlock parameterBlock = getBiosParameterBlock();
        getDevice().write(reinterpret_cast<const uint8_t *>(fat + index * 2 / getDevice().getSectorSize()), parameterBlock.reservedSectorCount + i * parameterBlock.sectorsPerFat + index * 2 / getDevice().getSectorSize(), 1);
    }
}

FileAllocationTable::Type FileAllocationTable16::getType() {
    return FileAllocationTable::FAT16;
}
