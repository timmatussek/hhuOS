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
#include <kernel/services/StorageService.h>
#include "PataDevice.h"
#include "PatapiDevice.h"
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

    auto *storageService = Kernel::getService<StorageService>();

    softwareReset(0x00);
    selectDrive(0x00, false);

    if(sectorNumberRegister.inb() == 0x01 && sectorCountRegister.inb() == 0x01) {
        uint8_t cl = cylinderLowRegister.inb();
        uint8_t ch = cylinderHighRegister.inb();

        if (cl == 0x00 && ch == 0x00) {
            if(PataDevice::isValid(*this, 0x00)) {
                log->info("Detected primary drive of type 'PATA'");
                storageService->registerDevice(new PataDevice(*this, 0x00));
            }
        } else if (cl == 0x14 && ch == 0xeb) {
            if(PatapiDevice::isValid(*this, 0x00)) {
                log->info("Detected primary drive of type 'PATAPI'");
                storageService->registerDevice(new PatapiDevice(*this, 0x00));
            }
        } else if (cl == 0x3c && ch == 0xc3) {
            log->info("Detected primary drive of type 'SATA'");
        } else if (cl == 0x69 && ch == 0x96) {
            log->info("Detected primary drive of type 'SATAPI'");
        }
    }

    softwareReset(0x01);
    selectDrive(0x01, false);

    if(sectorNumberRegister.inb() == 0x01 && sectorCountRegister.inb() == 0x01) {
        uint8_t cl = cylinderLowRegister.inb();
        uint8_t ch = cylinderHighRegister.inb();

        if (cl == 0x00 && ch == 0x00) {
            if(PataDevice::isValid(*this, 0x01)) {
                log->info("Detected secondary drive of type 'PATA'");
                storageService->registerDevice(new PataDevice(*this, 0x01));
            }
        } else if (cl == 0x14 && ch == 0xeb) {
            if(PatapiDevice::isValid(*this, 0x00)) {
                log->info("Detected secondary drive of type 'PATAPI'");
                storageService->registerDevice(new PatapiDevice(*this, 0x01));
            }
        } else if (cl == 0x3c && ch == 0xc3) {
            log->info("Detected secondary drive of type 'SATA'");
        } else if (cl == 0x69 && ch == 0x96) {
            log->info("Detected secondary drive of type 'SATAPI'");
        }
    }
}

bool AtaController::selectDrive(uint8_t driveNumber, bool setLba, uint8_t lbaHighest) {
    // Check for invalid drive number
    if(driveNumber > 1) {
        return false;
    }

    lbaHighest = static_cast<uint8_t>(lbaHighest & 0x0f);

    auto outByte = static_cast<uint8_t>(0xa0 | driveNumber << 4 | lbaHighest);

    if(setLba) {
        outByte |= 0x40;
    }

    // Check if drive is already selected
    if(outByte == lastSelectByte) {
        return true;
    }

    // Select drive; Afterwards, one should wait at least 400ns
    driveHeadRegister.outb(outByte);
    timeService->msleep(1);

    lastSelectByte = outByte;

    return true;
}

bool AtaController::waitForNotBusy(IoPortWrapper &port) {
    uint32_t time = timeService->getSystemTime();

    while(port.inb() & 0x80) {
        // Check for timeout
        if(timeService->getSystemTime() >= time + ATA_TIMEOUT) {
            return false;
        }
    }

    return true;
}

bool AtaController::waitForReady(IoPortWrapper &port) {
    uint32_t time = timeService->getSystemTime();

    while(!(port.inb() & 0x40)) {
        // Check for timeout
        if(timeService->getSystemTime() >= time + ATA_TIMEOUT) {
            return false;
        }
    }

    return true;
}

bool AtaController::waitForDrq(IoPortWrapper &port) {
    uint32_t time = timeService->getSystemTime();

    while(!(port.inb() & 0x08)) {
        // Check for timeout
        if(timeService->getSystemTime() >= time + ATA_TIMEOUT) {
            return false;
        }
    }

    return true;
}

bool AtaController::softwareReset(uint8_t driveNumber) {
    selectDrive(driveNumber, false);

    // Perform software reset; Afterwards, one should wait at least 5us
    deviceControlRegister.outb(0x04);
    timeService->msleep(1);

    // Clear the reset bit and disable interrupts; Afterwards, one should wait at least 2 ms
    deviceControlRegister.outb(0x02);
    timeService->msleep(2);

    if(!waitForNotBusy(alternateStatusRegister)) {
        return false;
    }

    timeService->msleep(5);

    lastSelectByte = 0x00;

    // Check for errors
    return errorRegister.inb() == 0;
}

void AtaController::acquireControllerLock() {
    controllerLock.acquire();
}

void AtaController::releaseControllerLock() {
    controllerLock.release();
}