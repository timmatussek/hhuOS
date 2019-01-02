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

AtaIsaDriver::AtaIsaDriver(bool primaryController, bool secondaryController) {
    log = &Logger::get("ATA");

    if(primaryController) {
        if (checkDrive(COMMAND_BASE_PORT_1, CONTROL_BASE_PORT_1, 0) ||
            checkDrive(COMMAND_BASE_PORT_1, CONTROL_BASE_PORT_1, 1)) {

            log->info("Found primary controller using the default ISA ports; command port: 0x%08x,"
                      "control port: 0x%08x", COMMAND_BASE_PORT_1, CONTROL_BASE_PORT_1);

            primaryController = new AtaController(COMMAND_BASE_PORT_1, CONTROL_BASE_PORT_1, false);
        }
    }

    if(secondaryController) {
        if (checkDrive(COMMAND_BASE_PORT_2, CONTROL_BASE_PORT_2, 0) ||
            checkDrive(COMMAND_BASE_PORT_2, CONTROL_BASE_PORT_2, 1)) {

            log->info("Found secondary controller using the default ISA ports; command port: 0x%08x,"
                      "control port: 0x%08x", COMMAND_BASE_PORT_2, CONTROL_BASE_PORT_2);

            secondaryController = new AtaController(COMMAND_BASE_PORT_2, CONTROL_BASE_PORT_2, false);
        }
    }
}

bool AtaIsaDriver::isAvailable(bool primaryController, bool secondaryController) {
    if(primaryController) {
        if (checkDrive(COMMAND_BASE_PORT_1, CONTROL_BASE_PORT_1, 0) ||
            checkDrive(COMMAND_BASE_PORT_1, CONTROL_BASE_PORT_1, 1)) {
            
            return true;
        }
    }

    if(secondaryController) {
        if (checkDrive(COMMAND_BASE_PORT_2, CONTROL_BASE_PORT_2, 0) ||
            checkDrive(COMMAND_BASE_PORT_2, CONTROL_BASE_PORT_2, 1)) {
            
            return true;
        }
    }

    return false;
}

bool AtaIsaDriver::checkDrive(AtaCommandBasePort commandBasePort, AtaControlBasePort controlBasePort,
                               uint8_t driveNumber) {
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

    // Select drive; Afterwards, one should wait at least 400ns
    driveHeadRegister.outb(driveNumber << 4);
    timeService->msleep(1);

    // Reset drive; Afterwards, one should wait at least 5us
    deviceControlRegister.outb(0x04);
    timeService->msleep(1);

    // Clear the reset bit and disable interrupts; Afterwards, one should wait at least 2 ms
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