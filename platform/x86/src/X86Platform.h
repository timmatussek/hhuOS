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

#ifndef HHUOS_X86PLATFORM_H
#define HHUOS_X86PLATFORM_H

#include <kernel/multiboot/Constants.h>
#include <util/memory/String.h>
#include <Platform.h>
#include <kernel/interrupt/InterruptHandler.h>
#include <kernel/memory/PageDirectory.h>
#include <kernel/memory/VirtualAddressSpace.h>
#include <kernel/memory/manager/PageFrameAllocator.h>
#include <kernel/memory/manager/PagingAreaManager.h>
#include <kernel/memory/manager/IOMemoryManager.h>

class X86Platform : public Platform, public Kernel::InterruptHandler {

public:

    struct TaskStateSegment {
        uint32_t prev_tss;
        uint32_t esp0;      // Points to kernel stack
        uint32_t ss0;       // Points to segment used by kernel stack
        uint32_t esp1;
        uint32_t ss1;
        uint32_t esp2;
        uint32_t ss2;
        uint32_t cr3;
        uint32_t eip;
        uint32_t eflags;
        uint32_t eax;
        uint32_t ecx;
        uint32_t edx;
        uint32_t ebx;
        uint32_t esp;
        uint32_t ebp;
        uint32_t esi;
        uint32_t edi;
        uint32_t es;
        uint32_t cs;
        uint32_t ss;
        uint32_t ds;
        uint32_t fs;
        uint32_t gs;
        uint32_t ldt;
        uint16_t trap;
        uint16_t iomap_base;
    } __attribute__((packed));

    enum BlockType : uint8_t {
        MULTIBOOT_RESERVED = 0x00,
        HEAP_RESERVED = 0x01,
        PAGING_RESERVED = 0x02
    };

    struct MemoryBlock {
        uint32_t startAddress;
        uint32_t virtualStartAddress;
        uint32_t lengthInBytes;
        uint32_t blockCount;
        BlockType type;
    };

public:
    /**
     * Default Constructor.
     */
    X86Platform();

    /**
     * Copy constructor.
     */
    X86Platform(const X86Platform &other) = delete;

    /**
     * Assignment operator.
     */
    X86Platform &operator=(const X86Platform &other) = delete;

    /**
     * Destructor.
     */
    ~X86Platform() override = default;

    static X86Platform &getInstance();

    static void initializeGlobalDescriptorTables(uint16_t *systemGdt, uint16_t *biosGdt, uint16_t *systemGdtDescriptor, uint16_t *biosGdtDescriptor, uint16_t *PhysicalGdtDescriptor);

    static void createGlobalDescriptorTableEntry(uint16_t *gdt, uint16_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags);

    static void copyMultibootInfo(Kernel::Multiboot::Info *source, uint8_t *destination, uint32_t maxBytes);

    static void readMemoryMap(Kernel::Multiboot::Info *multibootInfo);

    static void initializeSystem(Kernel::Multiboot::Info *multibootInfo);

    static bool isInitialized();

    static TaskStateSegment& getTaskStateSegment();

    static MemoryBlock* getBlockMap();

    static Kernel::MemoryManager* getKernelHeapManager();

    void trigger(Kernel::InterruptFrame &frame) override;

    virtual void initialize();

    void calculateTotalPhysicalMemory();

    /**
     * Protects kernel pages from being written on.
     */
    void writeProtectKernelCode();

    void* getPhysicalAddress(void *virtAddress);

    // allocations and deletions of page tables

    /**
     * Allocates space in PageTableArea.
     *
     * @return Adress of the allocated page
     */
    void *allocPageTable();

    /**
     * Frees a Page Table / Directory.
     *
     * @param virtTableAddress Address of the table/directory that should be freed.
     */
    void freePageTable(void *virtTableAddress);

    /**
     * Creates Page Table for a non present entry in Page Directory
     *
     * @param dir Page Directory where Table should be mapped
     * @param idx Index into the Page Directory
     * @return uint32_t Return Value
     */
    void createPageTable(Kernel::PageDirectory *dir, uint32_t idx);


    // address space management

    /**
     * Creates a new virtual address space and the required memory managers.
     *
     * @return Pointer to the new address space
     */
    Kernel::VirtualAddressSpace *createAddressSpace(uint32_t managerOffset, const Util::Memory::String &managerType);

    /**
     * Switches to a given address space.
     *
     * @param addressSpace Pointer to the address spaces that should be switched to
     */
    void switchAddressSpace(Kernel::VirtualAddressSpace *addressSpace);

    /**
     * Remove an address space from the system
     *
     * @param adressSpace AdressSpace that should be removed
     */
    void removeAddressSpace(Kernel::VirtualAddressSpace *addressSpace);


    // Mappings and unmappings

    /**
     * Maps a page into the current page directory at a given virtual address.
     *
     * @param virtAddress Virtual address where a page should be mapped
     * @param flags Flags for Page Table Entry
     */
    void map(uint32_t virtAddress, uint16_t flags);

    /**
     * Maps a page at a given physical address to a virtual address.
     * The physical address should be allocated right now, since this function does
     * only map it!
     *
     * @param virtAddress Virtual address where a page should be mapped
     * @param flags Flags for Page Table entry
     * @param physAddress Physical address that should be mapped
     */
    void map(uint32_t virtAddress, uint16_t flags, uint32_t physAddress);

    /**
     * Range map function to map a range of virtual addresses into the current Page
     * Directory .
     *
     * @param virtStartAddress Virtual start address of the mapping
     * @param virtEndAddress Virtual start address of the mapping
     * @param flags Flags for the Page Table entries
     */
    void map(uint32_t virtStartAddress, uint32_t virtEndAddress, uint16_t flags);

    /**
     * Maps a physical address into the IO-space of the system, located at the upper
     * end of the virtual memory. The allocated memory is 4KB-aligned, therefore the
     * returned virtual memory address is also 4KB-aligned. If the given physical
     * address is not 4KB-aligned, one has to add a offset to the returned virtual
     * memory address in order to obtain the corresponding virtual address.
     *
     * @param physAddr Physical address to be mapped. This address is usually given
     *                 by a hardware device (e.g. for the LFB). If the pyhsAddr lies
     *                 in the address rang of the installed physical memory of the system,
     *                 please make sure you allocated that memory before!
     * @param size Size of memory to be allocated
     * @return Pointer to virtual IO memory block
     */
    void *mapIO(uint32_t physAddress, uint32_t size);

    /**
     * Unmap Page at a given virtual address.
     *
     * @param virtAddress Virtual Address to be unmapped
     * @return uint32_t Physical Address of unmapped page
     */
    uint32_t unmap(uint32_t virtAddress);

    /**
     * Unmap a range of virtual addresses in current Page Directory
     *
     * @param startVirtAddress Virtual start address to be unmapped
     * @param endVirtAddress last address to be unmapped
     * @return uint32_t Physical address of the last unmapped page
     */
    uint32_t unmap(uint32_t virtStartAddress, uint32_t virtEndAddress);

    /**
     * Maps IO-space for a device and allocates physical memory for it. All
     * allocations are 4KB-aligned.
     *
     * @param size Size of IO-memory to be allocated
     * @return Pointer to virtual IO memory block
     */
    void *mapIO(uint32_t size);

    /**
     * Free the IO-space described by the given IOMemInfo Block
     *
     * @param ptr Pointer to virtual IO memory that should be freed
     */
    void freeIO(void *ptr);

    [[nodiscard]] void* alloc(uint32_t size, uint32_t alignment) override;

    void free(void *ptr, uint32_t alignment) override;

    static MemoryBlock blockMap[256];

protected:

    static Kernel::MemoryManager *kernelMemoryManager;

private:

    static TaskStateSegment taskStateSegment;
    static Kernel::Multiboot::MemoryMapEntry memoryMap[256];
    static uint32_t memoryMapSize;
    static bool initialized;

    uint32_t totalPhysicalMemory = 0;
    // base page directory for for kernel mappings -> these mappings have to
    // appear in each process` page directory
    Kernel::PageDirectory *basePageDirectory = nullptr;
    // pointer to the currently active address space
    Kernel::VirtualAddressSpace *currentAddressSpace = nullptr;
    // Page frame Allocator to alloc physical memory in 4KB-blocks
    Kernel::PageFrameAllocator *pageFrameAllocator = nullptr;
    // Paging Area Manager to manage the virtual memory reserved for page tables
    // and directories
    Kernel::PagingAreaManager *pagingAreaManager = nullptr;
    // IO memory manager
    Kernel::IOMemoryManager *ioMemManager = nullptr;

    // list of all address spaces
    Util::Data::ArrayList<Kernel::VirtualAddressSpace*> *addressSpaces = nullptr;

};

#endif