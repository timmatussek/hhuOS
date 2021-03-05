/*
 * Copyright (C) 2018-2021 Heinrich-Heine-Universitaet Duesseldorf,
 * Institute of Computer Science, Department Operating Systems
 * Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Michael Schoettner
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

#ifndef HHUOS_DEVICE_BIOS_H
#define HHUOS_DEVICE_BIOS_H

#include <cstdint>

namespace Device {

/**
 * Provides static functions, that allow performing BIOS calls in Protected Mode
 */
class Bios {

public:
    /**
     * State of the 16-bit registers, that shall be loaded once the switch to real mode has been performed.
     * This way, parameters can be passed to a BIOS call.
     */
    struct CallParameters {
        uint16_t ds;
        uint16_t es;
        uint16_t fs;
        uint16_t flags;
        uint32_t di;
        uint32_t si;
        uint32_t bp;
        uint32_t sp;
        uint32_t bx;
        uint32_t dx;
        uint32_t cx;
        uint32_t ax;
    } __attribute__((packed));

public:
    /**
     * Default-Constructor.
     * Deleted, as this class has only static members.
     */
    Bios() = delete;

    /**
     * Copy constructor.
     */
    Bios(const Bios &other) = delete;

    /**
     * Assignment operator.
     */
    Bios &operator=(const Bios &other) = delete;

    /**
     * Destructor.
     */
    ~Bios() = default;

    /**
     * Initializes segment for bios call.
     * Builds up a 16-bit code segment manually. The start address of this code segment is in the GDT for bios calls.
     */
    static void initialize();

    /**
     * Provides a bios call via software interrupt
     *
     * @param Interrupt number number of the bios call
     * @param callParameters Parameter struct for the bios call
     */
    static void interrupt(int interruptNumber, const CallParameters &callParameters);

private:

    static const CallParameters *parameters;
};

}

#endif
