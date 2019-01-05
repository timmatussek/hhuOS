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

#ifndef HHUOS_ATACONTROLLER_H
#define HHUOS_ATACONTROLLER_H

#include <kernel/services/TimeService.h>
#include <lib/IoPortWrapper.h>
#include <cstdint>
#include <stdint-gcc.h>

class AtaController {

public:

    friend class AtaIsaDriver;
    friend class AtaPciDriver;
    friend class AtaDevice;
    friend class PataDevice;
    friend class PatapiDevice;

    static bool DEFAULT_PRIMARY_PORTS_IN_USE;
    static bool DEFAULT_SECONDARY_PORTS_IN_USE;

public:

    AtaController(uint32_t commandBasePort, uint32_t controlBasePort, bool useMmio);

    AtaController(const AtaController &copy) = delete;

    ~AtaController() = default;

    void acquireControllerLock();

    void releaseControllerLock();

private:

    void selectDrive(uint8_t driveNumber);

    bool busyWait();

    bool softwareReset(uint8_t driveNumber);

private:

    Logger *log = nullptr;

    TimeService *timeService = nullptr;

    Spinlock controllerLock;

    uint8_t selectedDrive = 0xff;

    // Command registers
    IoPortWrapper dataRegister;
    IoPortWrapper errorRegister;
    IoPortWrapper featureRegister;
    IoPortWrapper sectorCountRegister;
    IoPortWrapper sectorNumberRegister;
    IoPortWrapper cylinderLowRegister;
    IoPortWrapper cylinderHighRegister;
    IoPortWrapper lbaLowRegister;
    IoPortWrapper lbaMidRegister;
    IoPortWrapper lbaHighRegister;
    IoPortWrapper driveHeadRegister;
    IoPortWrapper statusRegister;
    IoPortWrapper commandRegister;

    // Control registers
    IoPortWrapper alternateStatusRegister;
    IoPortWrapper deviceControlRegister;
    IoPortWrapper driveAddressRegister;

    static const constexpr uint32_t ATA_TIMEOUT = 100;
};

#endif
