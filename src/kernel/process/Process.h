#ifndef HHUOS_PROCESS_H
#define HHUOS_PROCESS_H

#include <lib/system/IdGenerator.h>
#include <kernel/thread/Thread.h>
#include "kernel/memory/VirtualAddressSpace.h"
#include "ThreadScheduler.h"

namespace Kernel {

class Process {

    friend class ProcessScheduler;

private:

    static IdGenerator idGenerator;

    uint32_t id;
    uint8_t priority = 4;

    VirtualAddressSpace &addressSpace;

    ThreadScheduler *scheduler;

public:

    static void loadExecutable(const String &path);

    explicit Process(Kernel::VirtualAddressSpace &addressSpace);

    Process(const Process &other) = delete;

    Process &operator=(const Process &other) = delete;

    bool operator==(const Process &other);

    ~Process() = default;

    uint8_t getPriority() const;

    void setPriority(uint8_t priority);

    Kernel::VirtualAddressSpace& getAddressSpace() const;

    Kernel::Thread& getCurrentThread() const;

    void ready(Thread &thread);

    ThreadScheduler& getScheduler() const;

    void start();

private:

    // void yieldThread();
};

}

#endif
