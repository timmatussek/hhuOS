#include <lib/system/priority/AccessArrayPriorityPattern.h>
#include <kernel/core/Management.h>
#include <kernel/core/SystemCall.h>
#include "ProcessScheduler.h"

namespace Kernel {

extern "C" {
    void setSchedInit();
    void startFirstThread(Context *first);
}

ProcessScheduler::ProcessScheduler(PriorityPattern &priorityPattern) : priorityPattern(priorityPattern), readyQueues(priorityPattern.getPriorityCount()) {
    SystemCall::registerSystemCall(Standard::System::Call::SCHEDULER_YIELD, [](uint32_t paramCount, va_list params, Standard::System::Result *result) {
        if (ProcessScheduler::getInstance().isInitialized()) {
            ProcessScheduler::getInstance().yieldThreadSafe();
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

void ProcessScheduler::startUp() {
    lock.acquire();

    if (!isProcessWaiting()) {
        Cpu::throwException(Cpu::Exception::ILLEGAL_STATE, "Scheduler: No process is waiting to be scheduled!");
    }

    currentProcess = &getNextProcess();

    initialized = true;

    setSchedInit();

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

    dispatch(getNextProcess());
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

void ProcessScheduler::onTimerInterrupt(uint64_t timestampMs) {
    if(timestampMs - lastTimestampMs > 10) {
        lastTimestampMs = timestampMs;
        yieldThreadSafe();
    } else {
        currentProcess->yieldThread();
    }
}

void ProcessScheduler::yield() {
    if (!isProcessWaiting()) {
        lock.release();
        return;
    }

    dispatch(getNextProcess());
}

void ProcessScheduler::yieldThreadSafe() {
    if (!isProcessWaiting()) {
        return;
    }

    lock.acquire();

    dispatch(getNextProcess());
}

void ProcessScheduler::dispatch(Process &next) {
    if (!initialized) {
        Cpu::throwException(Cpu::Exception::ILLEGAL_STATE,
                            "Scheduler: 'dispatch' called but scheduler is not initialized!");
    }

    Thread &oldThread = currentProcess->getCurrentThread();

    currentProcess = &next;

    Kernel::Management::getInstance().switchAddressSpace(&next.getAddressSpace());

    currentProcess->scheduler->yield(oldThread);
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

bool ProcessScheduler::isInitialized() {
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