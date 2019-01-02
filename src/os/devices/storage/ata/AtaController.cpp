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

#include <kernel/Kernel.h>
#include "AtaIsaDriver.h"
#include "AtaController.h"

bool AtaController::DEFAULT_PRIMARY_PORTS_IN_USE = false;

bool AtaController::DEFAULT_SECONDARY_PORTS_IN_USE = false;

AtaController::AtaController(uint32_t commandBasePort, uint32_t controlBasePort, bool useMmio) :
        timeService(Kernel::getService<TimeService>()),
        dataRegister(commandBasePort + 0x00, useMmio),
        errorRegister(commandBasePort + 0x01, useMmio),
        featureRegister(commandBasePort + 0x01, useMmio),
        sectorCountRegister(commandBasePort + 0x02, useMmio),
        sectorNumberRegister(commandBasePort + 0x03, useMmio),
        cylinderLowRegister(commandBasePort + 0x04, useMmio),
        cylinderHighRegister(commandBasePort + 0x05, useMmio),
        lbaLowRegister(commandBasePort + 0x03, useMmio),
        lbaMidRegister(commandBasePort + 0x04, useMmio),
        lbaHighRegister(commandBasePort + 0x05, useMmio),
        driveHeadRegister(commandBasePort + 0x06, useMmio),
        statusRegister(commandBasePort + 0x07, useMmio),
        commandRegister(commandBasePort + 0x07, useMmio),
        alternateStatusRegister(controlBasePort + 0x02, useMmio),
        deviceControlRegister(controlBasePort + 0x02, useMmio),
        driveAddressRegister(controlBasePort + 0x03, useMmio) {

    if(commandBasePort == AtaIsaDriver::COMMAND_BASE_PORT_1 || controlBasePort == AtaIsaDriver::CONTROL_BASE_PORT_1) {
        DEFAULT_PRIMARY_PORTS_IN_USE = true;
    }

    if(commandBasePort == AtaIsaDriver::COMMAND_BASE_PORT_2 || controlBasePort == AtaIsaDriver::CONTROL_BASE_PORT_2) {
        DEFAULT_SECONDARY_PORTS_IN_USE = true;
    }

    log = &Logger::get("ATA");

    selectDrive(0x00);

    if(resetSelectedDrive()) {
        if (sectorCountRegister.inb() == 0x01 && sectorNumberRegister.inb() == 0x01) {
            log->info("Detected primary drive");
        }
    }

    selectDrive(0x01);

    if(resetSelectedDrive()) {
        if (sectorCountRegister.inb() == 0x01 && sectorNumberRegister.inb() == 0x01) {
            log->info("Detected secondary drive");
        }
    }
}

void AtaController::selectDrive(uint8_t driveNumber) {
    // Check for invalid drive number
    if(driveNumber > 1) {
        return;
    }

    // Select drive 0; Afterwards, one should wait at least 400ns
    driveHeadRegister.outb(driveNumber << 4);
    timeService->msleep(1);
}

bool AtaController::busyWait() {
    uint32_t time = timeService->getSystemTime();

    while(alternateStatusRegister.inb() & 0x80) {
        // Check for timeout
        if(timeService->getSystemTime() >= time + 100) {
            return false;
        }
    }

    return true;
}

bool AtaController::resetSelectedDrive() {
    // Reset selected drive; Afterwards, one should wait at least 5us
    deviceControlRegister.outb(0x04);
    timeService->msleep(1);

    // Clear the reset bit and disable interrupts; Afterwards, one should wait at least 2 ms
    deviceControlRegister.outb(0x02);
    timeService->msleep(2);

    if(!busyWait()) {
        return false;
    }

    // Check for errors
    return errorRegister.inb() == 0;

}