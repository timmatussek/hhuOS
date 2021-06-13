/*
 * Copyright (C) 2021 Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Michael Schoettner, Tim Matussek
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

#ifndef __LfsFlushCallback_include__
#define __LfsFlushCallback_include__

#include "Lfs.h"
#include "kernel/thread/KernelThread.h"

/**
 * Timeout between flushes in ms.
 */
#define FLUSH_INTERVAL 60000

/*
 * A thread to call flush at regular intervals.
 */
class LfsFlushCallback : public Kernel::KernelThread {
private:
    Lfs &lfs;

public:

    /**
     * Constructor.
     */
    LfsFlushCallback(Lfs &lfs) noexcept;

    /**
     * Overriding virtual function from KernelThread.
     */
    void run() override;
};

#endif
