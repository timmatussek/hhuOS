#include "FileAllocationTable32.h"

FileAllocationTable32::FileAllocationTable32(StorageDevice &device) : FileAllocationTable(device) {
    BiosParameterBlock &parameterBlock = getBiosParameterBlock();

    for(uint32_t i = 0; i < getBiosParameterBlock().fatCount; i++) {
        Util::SmartPointer<uint32_t> fat(new uint32_t[(parameterBlock.sectorsPerFat * device.getSectorSize()) / 4]);
        device.read(reinterpret_cast<uint8_t *>(fat.get()), parameterBlock.reservedSectorCount + i * parameterBlock.sectorsPerFat, parameterBlock.sectorsPerFat);
        tables.add(fat);
    }
}

uint32_t FileAllocationTable32::getEntry(uint32_t index) {
    return tables.get(0).get()[index];
}

void FileAllocationTable32::setEntry(uint32_t index, uint32_t value) {
    value &= 0x0fffffff;

    for(uint32_t i = 0; i < getBiosParameterBlock().fatCount; i++) {
        uint32_t *fat = tables.get(0).get();
        tables.get(0).get()[index] = value;

        BiosParameterBlock parameterBlock = getBiosParameterBlock();
        getDevice().write(reinterpret_cast<const uint8_t *>(fat + index * 2 / getDevice().getSectorSize()), parameterBlock.reservedSectorCount + i * parameterBlock.sectorsPerFat + index * 2 / getDevice().getSectorSize(), 1);
    }
}
