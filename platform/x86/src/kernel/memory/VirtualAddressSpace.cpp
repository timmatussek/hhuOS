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

#include <util/memory/Address.h>
#include <util/reflection/InstanceFactory.h>
#include <X86Platform.h>
#include "VirtualAddressSpace.h"
#include "MemLayout.h"
#include "Paging.h"
#include "VirtualAddressSpace.h"

namespace Kernel {

VirtualAddressSpace::VirtualAddressSpace(PageDirectory *basePageDirectory, uint32_t heapAddress, const Util::Memory::String &memoryManagerType) :
        managerType(memoryManagerType), heapAddress(Util::Memory::Address(heapAddress).alignUp(PAGESIZE)) {
    // create a new memory abstraction through paging
    this->pageDirectory = new PageDirectory(basePageDirectory);
    // the kernelspace heap manager is static and global for the system
    this->kernelSpaceHeapManager = X86Platform::getKernelHeapManager();
    // this is no bootstrap address space
    bootstrapAddressSpace = false;
}

VirtualAddressSpace::VirtualAddressSpace(PageDirectory *basePageDirectory, const Util::Memory::String &memoryManagerType) :
        managerType(memoryManagerType), heapAddress(2 * PAGESIZE) {
    if(basePageDirectory == nullptr) {
        this->pageDirectory = new PageDirectory();
    } else {
        // create a new memory abstraction through paging
        this->pageDirectory = new PageDirectory(basePageDirectory);
    }

    // the kernelspace heap manager is static and global for the system
    this->kernelSpaceHeapManager = X86Platform::getKernelHeapManager();
    // this is no bootstrap address space
    bootstrapAddressSpace = false;
}

VirtualAddressSpace::~VirtualAddressSpace() {
    // only delete things if they were allocated by the constructor
    if (!bootstrapAddressSpace) {
        delete pageDirectory;
        delete userSpaceHeapManager;
    }
}

void VirtualAddressSpace::init() {
    // create a new memory manager for userspace
    if (X86Platform::isInitialized()) {
        this->userSpaceHeapManager = (MemoryManager*) Util::Reflection::InstanceFactory::createInstance(managerType);
    }

    if (userSpaceHeapManager != nullptr) {
        userSpaceHeapManager->init(Util::Memory::Address(heapAddress).alignUp(PAGESIZE).get(), KERNEL_START - PAGESIZE);
    }

    initialized = true;
}

bool VirtualAddressSpace::isInitialized() const {
    return initialized;
}

}