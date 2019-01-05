#include "PatapiDevice.h"

PatapiDevice::PatapiDevice(AtaController &controller, uint8_t driveNumber) :
        AtaDevice(controller, driveNumber, generateCdName()) {
    memset(serialNumber, 0, sizeof(serialNumber));
    memset(firmareRevision, 0, sizeof(firmareRevision));
    memset(modelNumber, 0, sizeof(modelNumber));

    identify(0xa1);
}

bool PatapiDevice::isValid(AtaController &controller, uint8_t driveNumber) {
    auto *buf = new uint16_t[256];

    controller.acquireControllerLock();

    controller.selectDrive(driveNumber, false);

    controller.commandRegister.outw(0xa1);

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

    delete[] buf;

    return true;
}

bool PatapiDevice::read(uint8_t *buff, uint32_t sector, uint32_t count) {
    return false;
}

bool PatapiDevice::write(const uint8_t *buff, uint32_t sector, uint32_t count) {
    return false;
}
