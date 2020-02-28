#include <kernel/core/Management.h>
#include "ThreadScheduler.h"
#include "ProcessScheduler.h"

namespace Kernel {

extern "C" {
void switchContext(Context **current, Context **next);
}

ThreadScheduler::ThreadScheduler(PriorityPattern &priority) : currentThread(nullptr), priority(priority), readyQueues(priority.getPriorityCount()) {

}

void ThreadScheduler::ready(Thread &that) {

    if (that.hasStarted()) {
        Cpu::throwException(Cpu::Exception::ILLEGAL_STATE, "Scheduler: Trying to start an already running thread!");
    }

    if(currentThread == nullptr) {
        currentThread = &that;
    }

    lock.acquire();

    readyQueues[that.getPriority()].push(&that);

    lock.release();

    that.started = true;
}

void ThreadScheduler::exit() {

    currentThread->finished = true;

    lock.acquire();

    readyQueues[currentThread->getPriority()].remove(currentThread);

    lock.release();

    ProcessScheduler::getInstance().yield(false);
}

void ThreadScheduler::kill(Thread &that) {

    if (that.getId() == currentThread->getId()) {

        Cpu::throwException(Cpu::Exception::ILLEGAL_STATE,
                            "Scheduler: A thread is trying to kill itself... Use 'exit' instead!");
    }

    lock.acquire();

    readyQueues[that.getPriority()].remove(&that);

    lock.release();

    that.finished = true;
}

void ThreadScheduler::yield(Thread &oldThread, Process &nextProcess, bool tryLock) {

    if (tryLock) {
        if (!lock.tryAcquire()) {
            ProcessScheduler::getInstance().lock.release();
            return;
        }
    } else {
        lock.acquire();
    }

    Thread &nextThread = *getNextThread();

    lock.release();

    Kernel::Management::getInstance().switchAddressSpace(&nextProcess.getAddressSpace());

    Kernel::ProcessScheduler::getInstance().setCurrentProcess(nextProcess);

    dispatch(oldThread, nextThread);
}

void ThreadScheduler::block() {

    lock.acquire();

    readyQueues[currentThread->getPriority()].remove(currentThread);

    lock.release();

    ProcessScheduler::getInstance().yield(false);
}

void ThreadScheduler::deblock(Thread &that) {

    lock.acquire();

    readyQueues[that.getPriority()].push(&that);

    lock.release();
}

void ThreadScheduler::dispatch(Thread &current, Thread &next) {

    currentThread = &next;

    switchContext(&current.kernelContext, &next.kernelContext);
}

Thread &ThreadScheduler::getCurrentThread() {
    return *currentThread;
}

Thread *ThreadScheduler::getNextThread(bool tryLock) {

    if (!isThreadWaiting()) {

        lock.release();

        ProcessScheduler::getInstance().yieldFromThreadScheduler(tryLock);
    }

    Util::ArrayBlockingQueue<Thread *> *currentQueue = &readyQueues[priority.getNextPriority()];

    while (currentQueue->isEmpty()) {
        currentQueue = &readyQueues[priority.getNextPriority()];
    }

    Thread *ret = currentQueue->pop();

    currentQueue->push(ret);

    return ret;
}

bool ThreadScheduler::isThreadWaiting() {

    for (const auto &queue : readyQueues) {
        if (!queue.isEmpty()) {
            return true;
        }
    }

    return false;
}

uint32_t ThreadScheduler::getThreadCount() {

    uint32_t count = 0;

    for (const auto &queue : readyQueues) {
        count += queue.size();
    }

    return count;
}

uint8_t ThreadScheduler::getMaxPriority() {

    return static_cast<uint8_t>(readyQueues.length() - 1);
}

uint8_t ThreadScheduler::changePriority(Thread &thread, uint8_t priority) {

    return 0xff;
    // TODO
}

}