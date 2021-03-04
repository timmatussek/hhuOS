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

#include <Platform.h>
#include <device/bios/Bios.h>
#include <X86Platform.h>
#include <kernel/memory/MemLayout.h>

#include "X86BiosPlatform.h"

X86BiosPlatform *X86BiosPlatform::platform = nullptr;

X86BiosPlatform::X86BiosPlatform() : X86Platform() {}

Platform& Platform::getInstance() {
    return X86BiosPlatform::getInstance();
}

X86Platform& X86Platform::getInstance() {
    return X86BiosPlatform::getInstance();
}

X86BiosPlatform &X86BiosPlatform::getInstance() {
    auto blockMap = X86Platform::getBlockMap();

    if (X86BiosPlatform::platform == nullptr) {
        // create a static memory manager for the kernel heap
        for (uint32_t i = 0; blockMap[i].blockCount != 0; i++) {
            const auto &block = blockMap[i];

            if (block.type == X86Platform::HEAP_RESERVED) {
                static Kernel::FreeListMemoryManager heapMemoryManager;
                heapMemoryManager.init(block.virtualStartAddress, VIRT_KERNEL_HEAP_END);
                // set the kernel heap memory manager to this manager
                kernelMemoryManager = &heapMemoryManager;
                // use the new memory manager to alloc memory for the instance of SystemManegement
                platform = new X86BiosPlatform();
                break;
            }
        }
    }

    return *platform;
}

void X86BiosPlatform::initialize() {
    X86Platform::initialize();
    Device::Bios::initialize();
}

Util::Memory::String X86BiosPlatform::getName() const {
    return PLATFORM_NAME;
}
