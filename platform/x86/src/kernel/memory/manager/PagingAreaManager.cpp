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

#include <kernel/memory/Paging.h>
#include <kernel/memory/MemLayout.h>
#include "PagingAreaManager.h"

namespace Kernel {

PagingAreaManager::PagingAreaManager() : BitmapMemoryManager(PAGESIZE, true) {
    BitmapMemoryManager::init(VIRT_PAGE_MEM_START, VIRT_PAGE_MEM_END);

    managerType = PAGING_AREA_MANAGER;

    // We use already 256 Page Tables for Kernel mappings and one Page Directory as the Kernel´s PD
    bitmap->setRange(0, 8 * 32 + 2);

    freeMemory -= (8 * 32 * blockSize + 2 * blockSize);
}

void PagingAreaManager::init(uint32_t memoryStartAddress, uint32_t memoryEndAddress) {
    // Do nothing. The PagingAreaManager will always be initialized by the kernel and has hardcoded values.
}

Util::Memory::String PagingAreaManager::getClassName() {
    return CLASS_NAME;
}

}