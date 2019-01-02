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

#include "IoPortWrapper.h"

IoPortWrapper::IoPortWrapper(uint32_t address, bool useMmio) : useMmio(useMmio) {
    if(useMmio) {
        mmioAddress = reinterpret_cast<uint8_t *>(address);
    } else {
        ioPort = IOport(static_cast<uint16_t>(address));
    }
}

void IoPortWrapper::outb(uint8_t value) {
    if(useMmio) {
        *(mmioAddress) = value;
    } else {
        ioPort.outb(value);
    }
}

void IoPortWrapper::outw(uint16_t value) {
    if(useMmio) {
        *reinterpret_cast<uint16_t *>(mmioAddress) = value;
    } else {
        ioPort.outw(value);
    }
}

void IoPortWrapper::outdw(uint32_t value) {
    if(useMmio) {
        *reinterpret_cast<uint32_t *>(mmioAddress) = value;
    } else {
        ioPort.outdw(value);
    }
}

uint8_t IoPortWrapper::inb() {
    if(useMmio) {
        return *(mmioAddress);
    } else {
        return ioPort.inb();
    }
}

uint16_t IoPortWrapper::inw() {
    if(useMmio) {
        return *reinterpret_cast<uint16_t *>(mmioAddress);
    } else {
        return ioPort.inw();
    }
}

uint32_t IoPortWrapper::indw() {
    if(useMmio) {
        return *reinterpret_cast<uint32_t *>(mmioAddress);
    } else {
        return ioPort.indw();
    }
}
