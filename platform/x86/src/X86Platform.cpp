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

#include <kernel/multiboot/Structure.h>
#include <kernel/memory/MemLayout.h>
#include <kernel/interrupt/InterruptDispatcher.h>
#include <kernel/memory/Paging.h>
#include <util/memory/Address.h>
#include <util/elf/ElfConstants.h>
#include "X86Platform.h"
#include "asm_interface.h"

X86Platform::TaskStateSegment X86Platform::taskStateSegment{};
X86Platform::MemoryBlock X86Platform::blockMap[256]{};
Kernel::Multiboot::MemoryMapEntry X86Platform::memoryMap[256]{};
Kernel::MemoryManager *X86Platform::kernelMemoryManager = nullptr;
uint32_t X86Platform::memoryMapSize = 0;
bool X86Platform::initialized = false;

X86Platform::X86Platform() : Platform() {}

void X86Platform::initializeGlobalDescriptorTables(uint16_t *systemGdt, uint16_t *biosGdt, uint16_t *systemGdtDescriptor, uint16_t *biosGdtDescriptor, uint16_t *PhysicalGdtDescriptor) {
    // Set first 6 GDT entries to 0
    Util::Memory::Address<uint32_t>(systemGdt).setRange(0, 48);

    // Set first 4 bios GDT entries to 0
    Util::Memory::Address<uint32_t>(biosGdt).setRange(0, 32);

    // first set up general GDT for the system
    // first entry has to be null
    createGlobalDescriptorTableEntry(systemGdt, 0, 0, 0, 0, 0);
    // kernel code segment
    createGlobalDescriptorTableEntry(systemGdt, 1, 0, 0xFFFFFFFF, 0x9A, 0xC);
    // kernel data segment
    createGlobalDescriptorTableEntry(systemGdt, 2, 0, 0xFFFFFFFF, 0x92, 0xC);
    // user code segment
    createGlobalDescriptorTableEntry(systemGdt, 3, 0, 0xFFFFFFFF, 0xFA, 0xC);
    // user data segment
    createGlobalDescriptorTableEntry(systemGdt, 4, 0, 0xFFFFFFFF, 0xF2, 0xC);
    // tss segment
    createGlobalDescriptorTableEntry(systemGdt, 5, reinterpret_cast<uint32_t>(&getTaskStateSegment()), sizeof(TaskStateSegment), 0x89, 0x4);

    // set up descriptor for GDT
    *((uint16_t *) systemGdtDescriptor) = 6 * 8;
    // the normal descriptor should contain the virtual address of GDT
    *((uint32_t *) (systemGdtDescriptor + 1)) = (uint32_t) systemGdt + KERNEL_START;

    // set up descriptor for GDT with phys. address - needed for bootstrapping
    *((uint16_t *) PhysicalGdtDescriptor) = 6 * 8;
    // this descriptor should contain the physical address of GDT
    *((uint32_t *) (PhysicalGdtDescriptor + 1)) = (uint32_t) systemGdt;

    // now set up GDT for BIOS-calls (notice that no userspace entries are necessary here)
    // first entry has to be null
    createGlobalDescriptorTableEntry(biosGdt, 0, 0, 0, 0, 0);
    // kernel code segment
    createGlobalDescriptorTableEntry(biosGdt, 1, 0, 0xFFFFFFFF, 0x9A, 0xC);
    // kernel data segment
    createGlobalDescriptorTableEntry(biosGdt, 2, 0, 0xFFFFFFFF, 0x92, 0xC);
    // prepared BIOS-call segment (contains 16-bit code etc...)
    createGlobalDescriptorTableEntry(biosGdt, 3, 0x4000, 0xFFFFFFFF, 0x9A, 0x8);


    // set up descriptor for BIOS-GDT
    *((uint16_t *) biosGdtDescriptor) = 4 * 8;
    // the descriptor should contain physical address of BIOS-GDT because paging is not enabled during BIOS-calls
    *((uint32_t *) (biosGdtDescriptor + 1)) = (uint32_t) biosGdt;
}
void X86Platform::createGlobalDescriptorTableEntry(uint16_t *gdt, uint16_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    // each GDT-entry consists of 4 16-bit unsigned integers
    // calculate index into 16bit-array that represents GDT
    uint16_t idx = 4 * num;

    // first 16-bit value: [Limit 0:15]
    gdt[idx] = (uint16_t) (limit & 0xFFFF);
    // second 16-bit value: [Base 0:15]
    gdt[idx + 1] = (uint16_t) (base & 0xFFFF);
    // third 16-bit value: [Access Byte][Base 16:23]
    gdt[idx + 2] = (uint16_t) ((base >> 16) & 0xFF) | (access << 8);
    // fourth 16-bit value: [Base 24:31][Flags][Limit 16:19]
    gdt[idx + 3] = (uint16_t) ((limit >> 16) & 0x0F) | ((flags << 4) & 0xF0) | ((base >> 16) & 0xFF00);
}

void X86Platform::copyMultibootInfo(Kernel::Multiboot::Info *source, uint8_t *destination, uint32_t maxBytes) {
    auto destinationAddress = Util::Memory::Address<uint32_t>(destination, maxBytes);

    // first, copy the struct itself
    destinationAddress.copyRange(Util::Memory::Address<uint32_t>(source), sizeof(Kernel::Multiboot::Info));
    auto multibootInfo = reinterpret_cast<Kernel::Multiboot::Info*>(destinationAddress.get());
    destinationAddress = destinationAddress.add(sizeof(Kernel::Multiboot::Info));

    // then copy the commandline
    if(multibootInfo->flags & Kernel::Multiboot::MULTIBOOT_INFO_CMDLINE) {
        auto sourceAddress = Util::Memory::Address<uint32_t>(multibootInfo->commandLine);
        destinationAddress.copyString(sourceAddress);
        multibootInfo->commandLine = destinationAddress.get();
        destinationAddress = destinationAddress.add(sourceAddress.stringLength() + 1);
    }

    // TODO: the following rows may write past the end of our destination buf

    // then copy the module information
    if(multibootInfo->flags & Kernel::Multiboot::MULTIBOOT_INFO_MODS) {
        uint32_t length = multibootInfo->moduleCount * sizeof(Kernel::Multiboot::ModuleInfo);
        auto sourceAddress = Util::Memory::Address<uint32_t>(multibootInfo->moduleAddress, length);
        destinationAddress.copyRange(sourceAddress, length);
        multibootInfo->moduleAddress = destinationAddress.get();
        destinationAddress = destinationAddress.add(length);

        auto mods = reinterpret_cast<Kernel::Multiboot::ModuleInfo*>(multibootInfo->moduleAddress);
        for(uint32_t i = 0; i < multibootInfo->moduleCount; i++) {
            sourceAddress = Util::Memory::Address<uint32_t>(mods[i].string);
            destinationAddress.copyString(sourceAddress);
            mods[i].string = reinterpret_cast<char *>(destinationAddress.get());
            destinationAddress = destinationAddress.add(sourceAddress.stringLength() + 1);
        }
    }

    // then copy the symbol headers and the symbols
    // (currently disabled, so we just set it to 0)
    multibootInfo->symbols.elf.address = 0;
    /*if(multibootInfo->flags & Kernel::Multiboot::MULTIBOOT_INFO_ELF_SHDR) {
        uint32_t length = multibootInfo->symbols.elf.sectionSize * multibootInfo->symbols.elf.sectionCount;
        auto sourceAddress = Util::Memory::Address<uint32_t>(multibootInfo->symbols.elf.address, length);
        destinationAddress.copyRange(sourceAddress, length);
        multibootInfo->symbols.elf.address = destinationAddress.get();
        destinationAddress = destinationAddress.add(length);
        Symbols::copy(multibootInfo->symbols.elf, destinationAddress);
    }*/

    // then copy the memory map
    if(multibootInfo->flags & Kernel::Multiboot::MULTIBOOT_INFO_MEM_MAP) {
        auto sourceAddress = Util::Memory::Address<uint32_t>(multibootInfo->memoryMapAddress, multibootInfo->memoryMapLength);
        destinationAddress.copyRange(sourceAddress, multibootInfo->memoryMapLength);
        multibootInfo->memoryMapAddress = destinationAddress.get();
        destinationAddress = destinationAddress.add(multibootInfo->memoryMapLength);
    }

    // then copy the drives
    if(multibootInfo -> flags & Kernel::Multiboot::MULTIBOOT_INFO_DRIVE_INFO) {
        auto sourceAddress = Util::Memory::Address<uint32_t>(multibootInfo->driveAddress, multibootInfo->driveLength);
        destinationAddress.copyRange(sourceAddress, multibootInfo->driveLength);
        multibootInfo->driveAddress = destinationAddress.get();
        destinationAddress = destinationAddress.add(multibootInfo->driveLength);
    }

    // then copy the boot loader name
    if(multibootInfo->flags & Kernel::Multiboot::MULTIBOOT_INFO_BOOT_LOADER_NAME) {
        auto sourceAddress = Util::Memory::Address<uint32_t>(multibootInfo->bootloaderName);
        destinationAddress.copyString(sourceAddress);
        multibootInfo->bootloaderName = destinationAddress.get();
        destinationAddress = destinationAddress.add(sourceAddress.stringLength());
    }
}

void X86Platform::readMemoryMap(Kernel::Multiboot::Info *multibootInfo) {
    Kernel::Multiboot::Info tmp = *multibootInfo;
    Kernel::Multiboot::MemoryMapEntry *memory = (Kernel::Multiboot::MemoryMapEntry *) ((uint32_t) memoryMap - KERNEL_START);
    MemoryBlock *blocks = (MemoryBlock *) ((uint32_t) blockMap - KERNEL_START);
    uint32_t &mapSize = *((uint32_t *) ((uint32_t) &memoryMapSize - KERNEL_START));
    uint32_t kernelStart = (uint32_t) &___KERNEL_DATA_START__ - KERNEL_START;
    uint32_t kernelEnd = (uint32_t) &___KERNEL_DATA_END__ - KERNEL_START;

    memory[0] = {0x0, kernelStart, kernelEnd - kernelStart, Kernel::Multiboot::MULTIBOOT_MEMORY_RESERVED };
    uint32_t memoryIndex = 1;
    mapSize = 1;

    Kernel::Multiboot::ElfInfo &symbolInfo = tmp.symbols.elf;
    Elf::Constants::SectionHeader *sectionHeader = nullptr;

    if (tmp.flags & *VIRT2PHYS(&Kernel::Multiboot::MULTIBOOT_INFO_ELF_SHDR)) {
        for (uint32_t i = 0; i < symbolInfo.sectionCount; i++) {
            sectionHeader = (Elf::Constants::SectionHeader *) (symbolInfo.address + i * symbolInfo.sectionSize);
            if (sectionHeader->virtualAddress == 0x0) {
                continue;
            }

            uint32_t startAddress = sectionHeader->virtualAddress < KERNEL_START ? sectionHeader->virtualAddress : sectionHeader->virtualAddress - KERNEL_START;
            memory[memoryIndex] = { 0x0, startAddress, sectionHeader->size, Kernel::Multiboot::MULTIBOOT_MEMORY_RESERVED };
            memoryIndex++;
            mapSize++;
        }
    }

    if (tmp.flags & *VIRT2PHYS(&Kernel::Multiboot::MULTIBOOT_INFO_MODS)) {
        auto modInfo = (Kernel::Multiboot::ModuleInfo*) tmp.moduleAddress;
        for (uint32_t i = 0; i < tmp.moduleCount; i++) {
            memory[memoryIndex] = {0x0, modInfo[i].start, modInfo[i].end - modInfo[i].start, Kernel::Multiboot::MULTIBOOT_MEMORY_AVAILABLE };
            memoryIndex++;
            mapSize++;
        }
    }

    bool sorted;
    do {
        sorted = true;
        for (uint32_t i = 0; i < memoryIndex - 1; i++) {
            if (memory[i].address > memory[i + 1].address) {
                Kernel::Multiboot::MemoryMapEntry help = memory[i];
                memory[i] = memory[i + 1];
                memory[i + 1] = help;

                sorted = false;
            }
        }
    } while (!sorted);

    uint32_t blockIndex = 0;
    blocks[blockIndex] = {static_cast<uint32_t>(memory[0].address), 0, static_cast<uint32_t>(memory[0].length), 0, MULTIBOOT_RESERVED};

    for (uint32_t i = 1; i < memoryIndex; i++) {
        if (memory[i].address > blocks[blockIndex].startAddress + blocks[blockIndex].lengthInBytes + PAGESIZE) {
            blocks[++blockIndex] = {static_cast<uint32_t>(memory[i].address), 0, static_cast<uint32_t>(memory[i].length), 0, MULTIBOOT_RESERVED};
        } else if (memory[i].address + memory[i].length > blocks[blockIndex].startAddress + blocks[blockIndex].lengthInBytes) {
            blocks[blockIndex].lengthInBytes = (memory[i].address + memory[i].length) - blocks[blockIndex].startAddress;
        }
    }

    uint32_t alignment = 4 * 1024 * 1024;
    for (uint32_t i = 0; i <= blockIndex; i++) {
        // Align start address down to the beginning of the 4MB page (uses integer division)
        uint64_t tmpAddress = blocks[i].startAddress;
        blocks[i].startAddress = (blocks[i].startAddress / alignment) * alignment;
        blocks[i].lengthInBytes += tmpAddress - blocks[i].startAddress;

        blocks[i].blockCount = blocks[i].lengthInBytes % alignment == 0 ? (blocks[i].lengthInBytes / alignment) : (blocks[i].lengthInBytes / alignment + 1);
    }
}

X86Platform::TaskStateSegment &X86Platform::getTaskStateSegment() {
    return taskStateSegment;
}

X86Platform::MemoryBlock *X86Platform::getBlockMap() {
    return blockMap;
}

Kernel::MemoryManager *X86Platform::getKernelHeapManager() {
    return kernelMemoryManager;
}

bool X86Platform::isInitialized() {
    return initialized;
}

void X86Platform::initializeSystem(Kernel::Multiboot::Info *multibootInfo) {
    // Enable interrupts
    Device::Cpu::enableInterrupts();
    Kernel::Multiboot::Structure::init(multibootInfo);

    // Create an instance of the Platform and initialize it (sets up paging and system management)
    auto &platform = getInstance();
    platform.initialize();

    // Protect kernel code
    platform.writeProtectKernelCode();
    initialized = true;
}

void X86Platform::trigger(Kernel::InterruptFrame &frame) {
    // Get page fault address and flags
    uint32_t faultAddress;
    // the faulted linear address is loaded in the cr2 register
    asm ("mov %%cr2, %0" : "=r" (faultAddress));

    // There should be no access to the first page (address 0)
    if (faultAddress == 0) {
        Device::Cpu::throwException(Device::Cpu::Exception::NULL_POINTER);
    }

    // check if pagefault was caused by illegal page access
    if ((frame.error & 0x00000001u) > 0) {
        Device::Cpu::throwException(Device::Cpu::Exception::ILLEGAL_PAGE_ACCESS);
    }

    // Map the faulted Page
    map(faultAddress, PAGE_PRESENT | PAGE_READ_WRITE);
    // TODO: Check other Faults
}

void X86Platform::initialize() {
    // Physical Page Frame Allocator is initialized to be possible to allocate
    // physical memory (page frames)
    calculateTotalPhysicalMemory();
    pageFrameAllocator = new Kernel::PageFrameAllocator();
    pageFrameAllocator->init(0, totalPhysicalMemory);

    // to be able to map new pages, a bootstrap address space is created.
    // It uses only the basePageDirectory with mapping for kernel space
    currentAddressSpace = new Kernel::VirtualAddressSpace(nullptr);

    // Init Paging Area Manager -> Manages the virtual addresses of all page tables
    // and directories
    pagingAreaManager = new Kernel::PagingAreaManager();
    // create a Base Page Directory (used to map the kernel into every process)
    basePageDirectory = currentAddressSpace->getPageDirectory();

    // register Paging Manager to handle Page Faults
    Kernel::InterruptDispatcher::getInstance().assign(Kernel::InterruptDispatcher::PAGEFAULT, *this);

    // initialize io-memory afterwards, because pagefault will occur setting up the first list header
    // Init the manager for virtual IO Memory
    ioMemManager = new Kernel::IOMemoryManager();

    currentAddressSpace->init();
    switchAddressSpace(currentAddressSpace);

    // add first address space to list with all address spaces
    addressSpaces = new Util::Data::ArrayList<Kernel::VirtualAddressSpace*>;
    addressSpaces->add(currentAddressSpace);

    // Initialize global objects afterwards, because now missing pages can be mapped
    _init();
}

void X86Platform::calculateTotalPhysicalMemory() {
    Kernel::Multiboot::MemoryMapEntry &maxEntry = memoryMap[0];

    for (uint32_t i = 0; i < memoryMapSize; i++) {
        const auto &entry = memoryMap[i];
        if (entry.type != Kernel::Multiboot::MULTIBOOT_MEMORY_AVAILABLE) {
            continue;
        }

        if (entry.length > maxEntry.length) {
            maxEntry = entry;
        }
    }

    if (maxEntry.type != Kernel::Multiboot::MULTIBOOT_MEMORY_AVAILABLE) {
        Device::Cpu::throwException(Device::Cpu::Exception::ILLEGAL_STATE, "No usable memory found!");
    }

    totalPhysicalMemory = maxEntry.length > PHYS_MEM_CAP ? PHYS_MEM_CAP : maxEntry.length;
}

void X86Platform::writeProtectKernelCode() {
    basePageDirectory->writeProtectKernelCode();
}

void* X86Platform::getPhysicalAddress(void *virtAddress) {
    return currentAddressSpace->getPageDirectory()->getPhysicalAddress(virtAddress);
}

void *X86Platform::allocPageTable() {
    return pagingAreaManager->alloc(PAGESIZE);
}

void X86Platform::freePageTable(void *virtTableAddress) {
    void *physAddress = getPhysicalAddress(virtTableAddress);
    // free virtual memory
    pagingAreaManager->free(virtTableAddress);
    // free physical memory
    pageFrameAllocator->free(physAddress);
}

void X86Platform::createPageTable(Kernel::PageDirectory *dir, uint32_t idx) {
    // get some virtual memory for the table
    void *virtAddress = allocPageTable();
    // get physical memory for the table
    void *physAddress = getPhysicalAddress(virtAddress);
    // there must be no mapping from virtual to physical address be done here,
    // because the page is zeroed out after allocation by the PagingAreaManager

    // create the table in the page directory
    dir->createTable(idx, (uint32_t) physAddress, (uint32_t) virtAddress);
}

void X86Platform::map(uint32_t virtAddress, uint16_t flags) {
    // allocate a physical page frame where the page should be mapped
    auto physAddress = (uint32_t) pageFrameAllocator->alloc(PAGESIZE);
    // map the page into the directory
    currentAddressSpace->getPageDirectory()->map(physAddress, virtAddress, flags);
}

void X86Platform::map(uint32_t virtAddress, uint16_t flags, uint32_t physAddress) {
    // map the page into the directory
    currentAddressSpace->getPageDirectory()->map(physAddress, virtAddress, flags);
}

void X86Platform::map(uint32_t virtStartAddress, uint32_t virtEndAddress, uint16_t flags) {
    // get 4KB-aligned start and end address
    uint32_t alignedStartAddress = virtStartAddress & 0xFFFFF000;
    uint32_t alignedEndAddress = virtEndAddress & 0xFFFFF000;
    alignedEndAddress += (virtEndAddress % PAGESIZE == 0) ? 0 : PAGESIZE;
    // map all pages
    for (uint32_t i = alignedStartAddress; i < alignedEndAddress; i += PAGESIZE) {
        map(i, flags);
    }
}

uint32_t X86Platform::unmap(uint32_t virtAddress) {
    // request the pagedirectory to unmap the page
    uint32_t physAddress = currentAddressSpace->getPageDirectory()->unmap(virtAddress);
    if (!physAddress) {
        return 0;
    }

    pageFrameAllocator->free((void *) (physAddress));

    // invalidate entry in TLB
    asm volatile("push %%edx;"
                 "movl %0,%%edx;"
                 "invlpg (%%edx);"
                 "pop %%edx;"  : : "r"(virtAddress));

    return physAddress;
}

uint32_t X86Platform::unmap(uint32_t virtStartAddress, uint32_t virtEndAddress) {
    // remark: if given addresses are not aligned on pages, we do not want to unmap
    // data that could be on the same page before startVirtAddress or behind endVirtAddress

    // get aligned start and end address of the area to be freed
    uint32_t startVAddr = virtStartAddress & 0xFFFFF000;
    startVAddr += ((virtStartAddress % PAGESIZE != 0) ? PAGESIZE : 0);
    // calc start address of the last page we want to unmap
    uint32_t endVAddr = virtEndAddress & 0xFFFFF000;
    endVAddr -= (((virtEndAddress + 1) % PAGESIZE != 0) ? PAGESIZE : 0);

    // check if an unmap is possible (the start and end address have to contain
    // at least one complete page)
    if (endVAddr < virtStartAddress) {
        return 0;
    }
    // amount of pages to be unmapped
    uint32_t pageCnt = (endVAddr - startVAddr) / PAGESIZE + 1;

    // loop through the pages and unmap them
    uint32_t ret = 0;
    uint8_t cnt = 0;
    uint32_t i;
    for (i = 0; i < pageCnt; i++) {
        ret = unmap(startVAddr + i * PAGESIZE);

        if (!ret) {
            cnt++;
        } else {
            cnt = 0;
        }
        // if there were three pages after each other already unmapped, we break here
        // this is sort of a workaround because by merging large free memory blocks in memory management
        // it might happen that some parts of the memory are already unmapped
        if (cnt == 3) {
            break;
        }
    }


    return ret;
}

void* X86Platform::mapIO(uint32_t physAddress, uint32_t size) {
    // get amount of needed pages
    uint32_t pageCnt = size / PAGESIZE;
    pageCnt += (size % PAGESIZE == 0) ? 0 : 1;

    // allocate 4KB-aligned virtual IO-memory
    void *virtStartAddress = ioMemManager->alloc(size);

    // Check for nullpointer
    if (virtStartAddress == nullptr) {
        Device::Cpu::throwException(Device::Cpu::Exception::OUT_OF_MEMORY);
    }

    // map the allocated virtual IO memory to physical addresses
    for (uint32_t i = 0; i < pageCnt; i++) {
        // since the virtual memory is one block, we can update the virtual address this way
        uint32_t virtAddress = (uint32_t) virtStartAddress + i * PAGESIZE;

        // if the virtual address is already mapped, we have to unmap it
        // this can happen because the headers of the free list are mapped
        // to arbitrary physical addresses, but the IO Memory should be mapped
        // to given physical addresses
        X86Platform::getInstance().unmap(virtAddress);
        // map the page to given physical address

        map(virtAddress, PAGE_PRESENT | PAGE_READ_WRITE | PAGE_NO_CACHING, physAddress + i * PAGESIZE);
    }

    return virtStartAddress;
}

void* X86Platform::mapIO(uint32_t size) {
    // get amount of needed pages
    uint32_t pageCnt = size / PAGESIZE;
    pageCnt += (size % PAGESIZE == 0) ? 0 : 1;

    // allocate block of physical memory
    void *physStartAddress = pageFrameAllocator->alloc(size);

    // allocate 4KB-aligned virtual IO-memory
    void *virtStartAddress = ioMemManager->alloc(size);

    // check for nullpointer
    if (virtStartAddress == nullptr) {
        Device::Cpu::throwException(Device::Cpu::Exception::OUT_OF_MEMORY);
    }

    // map the allocated virtual IO memory to physical addresses
    for (uint32_t i = 0; i < pageCnt; i++) {
        // since the virtual memory is one block, we can update the virtual address this way
        uint32_t virtAddress = (uint32_t) virtStartAddress + i * PAGESIZE;

        // if the virtual address is already mapped, we have to unmap it
        // this can happen because the headers of the free list are mapped
        // to arbitrary physical addresses, but the IO Memory should be mapped
        // to given physical addresses
        X86Platform::getInstance().unmap(virtAddress);
        // map the page to given physical address

        map(virtAddress, PAGE_PRESENT | PAGE_READ_WRITE | PAGE_NO_CACHING, (uint32_t) physStartAddress + i * PAGESIZE);
    }

    return virtStartAddress;
}

void X86Platform::freeIO(void *ptr) {
    ioMemManager->free(ptr);
}

Kernel::VirtualAddressSpace* X86Platform::createAddressSpace(uint32_t managerOffset, const Util::Memory::String &managerType) {
    auto addressSpace = new Kernel::VirtualAddressSpace(basePageDirectory, managerOffset, managerType);
    // add to the list of address spaces
    addressSpaces->add(addressSpace);

    return addressSpace;
}

void X86Platform::switchAddressSpace(Kernel::VirtualAddressSpace *addressSpace) {
    // set current address space
    this->currentAddressSpace = addressSpace;
    // load cr3-register with phys. address of Page Directory
    load_page_directory(addressSpace->getPageDirectory()->getPageDirectoryPhysicalAddress());

    if (!this->currentAddressSpace->isInitialized()) {
        this->currentAddressSpace->init();
    }
}

void X86Platform::removeAddressSpace(Kernel::VirtualAddressSpace *addressSpace) {
    // the current active address space cannot be removed
    if (currentAddressSpace == addressSpace) {
        return;
    }
    // remove from list
    addressSpaces->remove(addressSpace);
    // call destructor
    delete addressSpace;
}

void *X86Platform::alloc(uint32_t size, uint32_t alignment) {
    return X86Platform::kernelMemoryManager->alloc(size, alignment);
}

void X86Platform::free(void *ptr, uint32_t alignment) {
    X86Platform::kernelMemoryManager->free(ptr, alignment);
}
