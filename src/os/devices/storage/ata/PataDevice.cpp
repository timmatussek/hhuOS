/*
 * Copyright (C) 2018 Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Michael Schoettner
 * Heinrich-Heine University
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include "PataDevice.h"

extern "C" {
#include <lib/libc/string.h>
}

PataDevice::PataDevice(AtaController &controller, uint8_t driveNumber) :
        AtaDevice(controller, driveNumber, generateHddName()) {
    memset(serialNumber, 0, sizeof(serialNumber));
    memset(firmareRevision, 0, sizeof(firmareRevision));
    memset(modelNumber, 0, sizeof(modelNumber));

    identify(0xec);
}

bool PataDevice::isValid(AtaController &controller, uint8_t driveNumber) {
    auto *buf = new uint16_t[256];

    controller.acquireControllerLock();

    controller.selectDrive(driveNumber);

    controller.commandRegister.outw(0xec);

    if(!controller.busyWait()) {
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

bool PataDevice::read(uint8_t *buff, uint32_t sector, uint32_t count) {
    return false;
}

bool PataDevice::write(const uint8_t *buff, uint32_t sector, uint32_t count) {
    return false;
}