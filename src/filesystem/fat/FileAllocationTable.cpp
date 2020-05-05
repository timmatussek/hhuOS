#include <lib/util/SmartPointer.h>
#include "FileAllocationTable.h"

FileAllocationTable::FileAllocationTable(StorageDevice &device) : device(device) {
    Util::SmartPointer<uint8_t> bootSector(new uint8_t[device.getSectorSize()]);

    if(!device.read(bootSector.get(), 0, 1)) {
        Cpu::throwException(Cpu::Exception::ILLEGAL_STATE, "FAT: Unable to read boot sector!");
    }

    memcpy(&biosParameterBlock, bootSector.get(), sizeof(BiosParameterBlock));
}

StorageDevice &FileAllocationTable::getDevice() {
    return device;
}

FileAllocationTable::BiosParameterBlock &FileAllocationTable::getBiosParameterBlock() {
    return biosParameterBlock;
}
