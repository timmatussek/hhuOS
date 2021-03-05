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
#include <X86Platform.h>
#include "kernel/multiboot/Structure.h"
#include "PageFrameAllocator.h"

namespace Kernel {

PageFrameAllocator::PageFrameAllocator() : BitmapMemoryManager(PAGESIZE, false) {
    managerType = PAGE_FRAME_ALLOCATOR;
}

void PageFrameAllocator::init(uint32_t memoryStartAddress, uint32_t memoryEndAddress) {
    BitmapMemoryManager::init(memoryStartAddress, memoryEndAddress);

    // Reserve blocks already used by system image and initrd
    const auto blockMap = X86Platform::getBlockMap();
    for (uint32_t i = 0; blockMap[i].blockCount != 0; i++) {
        const auto &block = blockMap[i];
        uint32_t start = block.startAddress / PAGESIZE;
        uint32_t length = block.blockCount * 1024;

        bitmap->setRange(start, length);
        freeMemory -= length * blockSize;
    }
}

Util::Memory::String PageFrameAllocator::getClassName() {
    return CLASS_NAME;
}

}