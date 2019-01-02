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

#ifndef HHUOS_ATAISADRIVER_H
#define HHUOS_ATAISADRIVER_H

#include "AtaController.h"

class AtaIsaDriver {

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

    AtaIsaDriver(bool primaryController, bool secondaryController);

    ~AtaIsaDriver() = default;

    static bool isAvailable(bool primaryController, bool secondaryController);

    static bool checkDrive(AtaCommandBasePort commandBasePort, AtaControlBasePort controlBasePort, uint8_t driveNumber);

private:

    Logger *log = nullptr;

    AtaController *primaryController = nullptr;
    AtaController *secondaryController = nullptr;

};

#endif
