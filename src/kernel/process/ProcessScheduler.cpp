#include <lib/system/priority/AccessArrayPriorityPattern.h>
#include <kernel/core/SystemCall.h>
#include "ProcessScheduler.h"

namespace Kernel {

extern "C" {
    void setSchedInit();
    uint32_t getSchedInit();
    void startFirstThread(Context *first);
    void releaseSchedulerLock();
}

void setSchedInit() {
    Kernel::ProcessScheduler::getInstance().setInitialized();
}

uint32_t getSchedInit() {
    return Kernel::ProcessScheduler::getInstance().isInitialized();
}

void releaseSchedulerLock() {
    Kernel::ProcessScheduler::getInstance().lock.release();
}

ProcessScheduler::ProcessScheduler(PriorityPattern &priorityPattern) : priorityPattern(priorityPattern), readyQueues(priorityPattern.getPriorityCount()) {
    SystemCall::registerSystemCall(Standard::System::Call::SCHEDULER_YIELD, [](uint32_t paramCount, va_list params, Standard::System::Result *result) {
        if (ProcessScheduler::getInstance().isInitialized()) {
            bool tryLock = false;

            if (paramCount > 0) {
                tryLock = va_arg(params, int) == 0 ? false : true;
            }

            ProcessScheduler::getInstance().yield(tryLock);
            result->setStatus(Standard::System::Result::OK);
        } else {
            result->setStatus(Standard::System::Result::NOT_INITIALIZED);
        }
    });
}

ProcessScheduler& ProcessScheduler::getInstance() noexcept {
    static AccessArrayPriorityPattern priority(5);

    static ProcessScheduler instance(priority);

    return instance;
}

void ProcessScheduler::start() {
    lock.acquire();

    if (!isProcessWaiting()) {
        Cpu::throwException(Cpu::Exception::ILLEGAL_STATE, "Scheduler: No process is waiting to be scheduled!");
    }

    currentProcess = &getNextProcess();

    startFirstThread(currentProcess->scheduler->getNextThread()->kernelContext);
}

void ProcessScheduler::ready(Process &process) {
    lock.acquire();

    readyQueues[process.getPriority()].push(&process);

    lock.release();
}

void ProcessScheduler::exit() {
    lock.acquire();

    if (!initialized) {
        Cpu::throwException(Cpu::Exception::ILLEGAL_STATE,
                            "Scheduler: 'exit' called but scheduler is not initialized!");
    }

    if (!isProcessWaiting()) {
        Cpu::throwException(Cpu::Exception::ILLEGAL_STATE, "Scheduler: No process is waiting to be scheduled!");
    }

    dispatch(getNextProcess(), false);
}

void ProcessScheduler::kill(Process &process) {
    lock.acquire();

    if (!initialized) {
        Cpu::throwException(Cpu::Exception::ILLEGAL_STATE,"Scheduler: 'kill' called but scheduler is not initialized!");
    }

    if (process == getCurrentProcess()) {
        Cpu::throwException(Cpu::Exception::ILLEGAL_STATE,"Scheduler: A process is trying to kill itself... Use 'exit' instead!");
    }

    readyQueues[process.getPriority()].remove(&process);

    lock.release();
}

void ProcessScheduler::yieldFromThreadScheduler(bool tryLock) {
    if (!isProcessWaiting()) {
        lock.release();
        return;
    }

    dispatch(getNextProcess(), tryLock);
}

void ProcessScheduler::yield(bool tryLock) {
    if (!isProcessWaiting()) {
        Cpu::throwException(Cpu::Exception::ILLEGAL_STATE, "No process is running!");
    }

    if(lock.tryAcquire()) {
        dispatch(getNextProcess(), tryLock);
    } else {
        return;
    }
}

void ProcessScheduler::dispatch(Process &next, bool tryLock) {
    if (!initialized) {
        Cpu::throwException(Cpu::Exception::ILLEGAL_STATE,
                            "Scheduler: 'dispatch' called but scheduler is not initialized!");
    }

    Thread &oldThread = currentProcess->getCurrentThread();

    next.scheduler->yield(oldThread, next, tryLock);
}

Process& ProcessScheduler::getNextProcess() {
    if (!isProcessWaiting()) {
        Cpu::throwException(Cpu::Exception::ILLEGAL_STATE, "Scheduler: No thread is waiting to be scheduled!");
    }

    Util::ArrayBlockingQueue<Process*> *currentQueue = &readyQueues[priorityPattern.getNextPriority()];

    while (currentQueue->isEmpty()) {
        currentQueue = &readyQueues[priorityPattern.getNextPriority()];
    }

    Process *next = currentQueue->pop();

    currentQueue->push(next);

    return *next;
}

void ProcessScheduler::setInitialized() {
    initialized = 0x123456;
}

uint32_t ProcessScheduler::isInitialized() const {
    return initialized;
}

bool ProcessScheduler::isProcessWaiting() {
    for (const auto &queue : readyQueues) {
        if (!queue.isEmpty()) {
            return true;
        }
    }

    return false;
}

uint32_t ProcessScheduler::getProcessCount() {

    uint32_t count = 0;

    for (const auto &queue : readyQueues) {
        count += queue.size();
    }

    return count;
}

uint8_t ProcessScheduler::getMaxPriority() {
    return static_cast<uint8_t>(readyQueues.length() - 1);
}

uint8_t ProcessScheduler::changePriority(Process &process, uint8_t priority) {

    priority = static_cast<uint8_t>((priority > getMaxPriority()) ? (getMaxPriority()) : priority);

    lock.acquire();

    if(process == getCurrentProcess()) {
        lock.release();

        return priority;
    }

    readyQueues[process.getPriority()].remove(&process);

    readyQueues[priority].push(&process);

    lock.release();

    return priority;
}

Process& ProcessScheduler::getCurrentProcess() {
    return *currentProcess;
}

void ProcessScheduler::setCurrentProcess(Process &process) {
    this->currentProcess = &process;
}

uint32_t ProcessScheduler::getThreadCount() {
    uint32_t count = 0;

    for(const auto &queue : readyQueues) {
        for(const auto *process : queue) {
            count += process->getScheduler().getThreadCount();
        }
    }

    return count;
}

}