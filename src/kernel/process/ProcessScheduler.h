#ifndef HHUOS_PROCESSSCHEDULER_H
#define HHUOS_PROCESSSCHEDULER_H

#include <lib/system/priority/PriorityPattern.h>
#include <lib/util/ArrayBlockingQueue.h>
#include <lib/system/IdGenerator.h>
#include "Process.h"

namespace Kernel {

class ProcessScheduler {

public:

    ProcessScheduler(const ProcessScheduler &copy) = delete;

    ProcessScheduler &operator=(const ProcessScheduler &copy) = delete;

    ~ProcessScheduler() = default;

    void setInitialized();

    uint32_t isInitialized() const;

    void start();

    void ready(Process &process);

    void exit();

    void kill(Process &process);

    bool isProcessWaiting();

    Process& getCurrentProcess();

    uint32_t getProcessCount();

    uint32_t getThreadCount();

    uint8_t changePriority(Process &process, uint8_t priority);

    uint8_t getMaxPriority();

    static ProcessScheduler &getInstance() noexcept;

    Spinlock lock;

private:

    friend class ThreadScheduler;

    explicit ProcessScheduler(PriorityPattern &priority);

    Process& getNextProcess();

    void setCurrentProcess(Process &process);

    void yieldFromThreadScheduler(bool tryLock);

    void yield(bool tryLock);

    void dispatch(Process &next, bool tryLock);

private:

    uint32_t initialized = 0;

    uint64_t lastTimestampMs = 0;

    Process *currentProcess = nullptr;

    PriorityPattern &priorityPattern;

    Util::Array<Util::ArrayBlockingQueue<Process*>> readyQueues;

};

}

#endif
