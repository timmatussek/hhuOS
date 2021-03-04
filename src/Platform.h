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

#ifndef HHUOS_PLATFORM_H
#define HHUOS_PLATFORM_H

#include <util/memory/String.h>
#include "GatesOfHell.h"

class Platform {

public:
    /**
     * Default-constructor.
     * Deleted, as this class has only static members.
     */
    Platform() = default;

    /**
     * Copy constructor.
     */
    Platform(const GatesOfHell &other) = delete;

    /**
     * Assignment operator.
     */
    Platform &operator=(const GatesOfHell &other) = delete;

    /**
     * Destructor.
     */
    virtual ~Platform() = default;

    static Platform& getInstance();

    [[nodiscard]] virtual Util::Memory::String getName() const = 0;

    [[nodiscard]] virtual void* alloc(uint32_t size, uint32_t alignment) = 0;

    virtual void free(void *ptr, uint32_t alignment) = 0;

private:

    static const constexpr char *PLATFORM_NAME = "Unknown";
};

#endif
