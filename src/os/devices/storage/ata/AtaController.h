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

#include <kernel/IOport.h>
#include <kernel/services/TimeService.h>

class AtaController {

public:

    enum AtaCommandBasePort : uint16_t {
        COMMAND_BASE_PORT_1 = 0x1f0,
        COMMAND_BASE_PORT_2 = 0x170
    };

    enum AtaControlBasePort : uint16_t {
        CONTROL_BASE_PORT_1 = 0x3f4,
        CONTROL_BASE_PORT_2 = 0x374
    };

public:

    AtaController(AtaCommandBasePort commandBasePort, AtaControlBasePort controlBasePort);

    AtaController(const AtaController &copy) = delete;

    ~AtaController() = default;

    static void setup();

private:

    static bool checkDrive(AtaController::AtaCommandBasePort commandBasePort,
                           AtaController::AtaControlBasePort controlBasePort, uint8_t driveNumber);

private:

    static Logger &log;

    TimeService *timeService = nullptr;

    // Command registers
    IOport dataRegister;
    IOport errorRegister;
    IOport featureRegister;
    IOport sectorCountRegister;
    IOport sectorNumberRegister;
    IOport cylinderLowRegister;
    IOport cylinderHighRegister;
    IOport lbaLowRegister;
    IOport lbaMidRegister;
    IOport lbaHighRegister;
    IOport driveHeadRegister;
    IOport statusRegister;
    IOport commandRegister;

    // Control registers
    IOport alternateStatusRegister;
    IOport deviceControlRegister;
    IOport driveAddressRegister;

};

#endif
