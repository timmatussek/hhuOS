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
#include <ctime>
#include "AtaController.h"

Logger &AtaController::log = Logger::get("ATA");

AtaController::AtaController(AtaCommandBasePort commandBasePort, AtaControlBasePort controlBasePort) :
        timeService(Kernel::getService<TimeService>()),
        dataRegister(commandBasePort + 0x00),
        errorRegister(commandBasePort + 0x01),
        featureRegister(commandBasePort + 0x01),
        sectorCountRegister(commandBasePort + 0x02),
        sectorNumberRegister(commandBasePort + 0x03),
        cylinderLowRegister(commandBasePort + 0x04),
        cylinderHighRegister(commandBasePort + 0x05),
        lbaLowRegister(commandBasePort + 0x03),
        lbaMidRegister(commandBasePort + 0x04),
        lbaHighRegister(commandBasePort + 0x05),
        driveHeadRegister(commandBasePort + 0x06),
        statusRegister(commandBasePort + 0x07),
        commandRegister(commandBasePort + 0x07),
        alternateStatusRegister(controlBasePort + 0x02),
        deviceControlRegister(controlBasePort + 0x02),
        driveAddressRegister(controlBasePort + 0x03) {

}

bool AtaController::checkDrive(AtaController::AtaCommandBasePort commandBasePort,
                               AtaController::AtaControlBasePort controlBasePort, uint8_t driveNumber) {
    auto *timeService = Kernel::getService<TimeService>();

    IOport errorRegister(commandBasePort + 0x01);
    IOport sectorCountRegister(commandBasePort + 0x02);
    IOport sectorNumberRegister(commandBasePort + 0x03);
    IOport driveHeadRegister(commandBasePort + 0x06);
    IOport alternateStatusRegister(commandBasePort + 0x02);
    IOport deviceControlRegister(controlBasePort + 0x02);

    // Check for invalid drive number
    if(driveNumber > 1) {
        return false;
    }

    // Select drive 0; Afterwards, one should wait at least 400ns
    driveHeadRegister.outb(driveNumber << 4);
    timeService->msleep(1);

    // Reset drive 0; Afterwards, one should wait at least 5us
    deviceControlRegister.outb(0x04);
    timeService->msleep(1);

    // Clear the reset bit and disable interrupts; Afterwards, one should wait at least 2 us
    deviceControlRegister.outb(0x02);
    timeService->msleep(2);

    // Wait for the busy bit to be clear
    uint32_t time = timeService->getSystemTime();

    while(alternateStatusRegister.inb() & 0x80) {
        // Check for timeout
        if(timeService->getSystemTime() >= time + 100) {
            return false;
        }
    }

    // Check for errors
    if(errorRegister.inb() != 0) {
        return false;
    }

    // Check sector count and sector number registers; If they both are set to 1, a drive is present
    return !(sectorCountRegister.inb() != 0x01 || sectorNumberRegister.inb() != 0x01);
}

void AtaController::setup() {
    bool found = false;

    // Check primary controller
    if(checkDrive(COMMAND_BASE_PORT_1, CONTROL_BASE_PORT_1, 0) || checkDrive(COMMAND_BASE_PORT_1, CONTROL_BASE_PORT_1, 1)) {
        log.info("Found primary ATA controller with at least one drive attached to it");
        found = true;

        new AtaController(COMMAND_BASE_PORT_1, CONTROL_BASE_PORT_1);
    }

    // Check secondary controller
    if(checkDrive(COMMAND_BASE_PORT_2, CONTROL_BASE_PORT_2, 0) || checkDrive(COMMAND_BASE_PORT_2, CONTROL_BASE_PORT_2, 1)) {
        log.info("Found secondary ATA controller with at least one drive attached to it");
        found = true;

        new AtaController(COMMAND_BASE_PORT_2, CONTROL_BASE_PORT_2);
    }

    if(!found) {
        log.info("No ATA drives available");
    }
}
