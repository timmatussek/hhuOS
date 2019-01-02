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

#ifndef HHUOS_IOPORTWRAPPER_H
#define HHUOS_IOPORTWRAPPER_H

#include <kernel/IOport.h>

class IoPortWrapper {

public:

    IoPortWrapper(uint32_t address, bool useMmio);

    IoPortWrapper(const IoPortWrapper &copy) = delete;

    ~IoPortWrapper() = default;

    void outb(uint8_t value);

    void outw(uint16_t value);

    void outdw(uint32_t value);

    uint8_t inb();

    uint16_t inw();

    uint32_t indw();

private:

    uint8_t *mmioAddress = nullptr;
    IOport ioPort = IOport(0x0000);

    bool useMmio = false;

};

#endif
