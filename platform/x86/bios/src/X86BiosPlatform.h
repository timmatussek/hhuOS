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

#ifndef HHUOS_X86BIOSPLATFORM_H
#define HHUOS_X86BIOSPLATFORM_H

#include <X86Platform.h>

class X86BiosPlatform : public X86Platform {

public:
    /**
     * Default Constructor.
     */
    X86BiosPlatform();

    /**
     * Copy constructor.
     */
    X86BiosPlatform(const X86BiosPlatform &other) = delete;

    /**
     * Assignment operator.
     */
    X86BiosPlatform& operator=(const X86BiosPlatform &other) = delete;

    /**
     * Destructor.
     */
    ~X86BiosPlatform() override = default;

    static X86BiosPlatform& getInstance();

    void initialize() override;

    [[nodiscard]] Util::Memory::String getName() const override;

private:

    static X86BiosPlatform *platform;

    static const constexpr char *PLATFORM_NAME = "x86-bios";
};

#endif