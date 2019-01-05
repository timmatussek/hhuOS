#include "AtaDevice.h"

AtaDevice::AtaDevice(AtaController &controller, uint8_t driveNumber, const String &name) : StorageDevice(name),
        log(&Logger::get("ATA")),
        controller(controller),
        driveNumber(driveNumber) {

}

bool AtaDevice::identify(uint8_t identifyCommand) {
    auto *buf = new uint16_t[256];

    controller.acquireControllerLock();

    controller.selectDrive(driveNumber, false);

    controller.commandRegister.outw(identifyCommand);

    if(!controller.waitForNotBusy(controller.alternateStatusRegister)) {
        controller.releaseControllerLock();

        return false;
    }

    if(controller.errorRegister.inb() != 0) {
        controller.releaseControllerLock();

        return false;
    }

    for(uint32_t i = 0; i < 256; i++) {
        buf[i] = controller.dataRegister.inw();
    }

    controller.releaseControllerLock();

    copyStringFromIdentifyBuffer(serialNumber, &buf[10], 10);
    copyStringFromIdentifyBuffer(firmareRevision, &buf[23], 4);
    copyStringFromIdentifyBuffer(modelNumber, &buf[27], 20);

    supportsChs = (buf[53] & 0x01) == 0x01;
    supportsLba28 = (buf[49] & 0x200) == 0x200;
    supportsLba48 = (buf[83] & 0x400) == 0x400;
    supportsDoubleWordIO = checkDoubleWordIO(buf, identifyCommand);

    if(supportsLba48) {
        sectorCount = ((uint64_t) buf[100]) + (((uint64_t) buf[101]) << 16) +
                (((uint64_t) buf[102]) << 16) + (((uint64_t) buf[103]) << 16);
    } else if(supportsLba28) {
        sectorCount = ((uint32_t) buf[60]) + (((uint32_t) buf[61]) << 16);
    } else if(supportsChs) {
        uint16_t cylinders = buf[54];
        uint16_t heads = buf[55];
        uint16_t sectorsPerTrack = buf[56];

        sectorCount = cylinders * heads * sectorsPerTrack;
    }

    sectorSize = buf[5];

    if(sectorSize == 0 && sectorCount != 0) {
        sectorSize = 512;
    }

    delete[] buf;

    log->info("Model number: %s", modelNumber);
    log->info("Serial number: %s", serialNumber);
    log->info("Firmware revision: %s", firmareRevision);
    log->info("Supports CHS: %s, Supports LBA28: %s, Supports LBA48: %s",
            supportsChs ? "true" : "false", supportsLba28 ? "true" : "false", supportsLba48 ? "true" : "false");
    log->info("Supports 32-Bit IO: %s", supportsDoubleWordIO ? "true" : "false");
    log->info("Sector size: %u, Sector count: %u", sectorSize, sectorCount);

    return true;
}

bool AtaDevice::checkDoubleWordIO(uint16_t *originalBuf, uint8_t identifyCommand) {
    auto *compareBuf = new uint32_t[512];

    controller.acquireControllerLock();

    controller.commandRegister.outw(identifyCommand);

    if(!controller.waitForNotBusy(controller.alternateStatusRegister)) {
        controller.releaseControllerLock();

        return false;
    }

    if(controller.errorRegister.inb() != 0) {
        controller.releaseControllerLock();

        return false;
    }

    for(uint32_t i = 0; i < 128; i++) {
        compareBuf[i] = controller.dataRegister.indw();
    }

    controller.releaseControllerLock();

    bool ret = memcmp(originalBuf, compareBuf, 512) == 0;

    delete[] compareBuf;

    return ret;
}

void AtaDevice::copyStringFromIdentifyBuffer(uint8_t *dest, uint16_t *src, uint32_t words) {
    for(uint32_t i = 0, j = 0; i < words; i += 2, ++j) {
        dest[i] = static_cast<uint8_t>(src[j] >> 8);
        dest[i + 1] = static_cast<uint8_t>(src[j] & 0xff);
    }
}

String AtaDevice::getHardwareName() {
    return (const char*) modelNumber;
}

uint32_t AtaDevice::getSectorSize() {
    return sectorSize;
}

uint64_t AtaDevice::getSectorCount() {
    return sectorCount;
}
