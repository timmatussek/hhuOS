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

#include "LfsFlushCallback.h"
#include "kernel/service/TimeService.h"
#include "kernel/core/System.h"

LfsFlushCallback::LfsFlushCallback(Lfs &lfs) noexcept : lfs(lfs) {}

void LfsFlushCallback::run() {
    Kernel::TimeService* timeService = Kernel::System::getService<Kernel::TimeService>();
    uint32_t lastTime = timeService->getSystemTime();
    while(true) {
        if(timeService->getSystemTime() - lastTime > FLUSH_INTERVAL) {
            lfs.flush();
            lastTime = timeService->getSystemTime();
        }
    }
}
