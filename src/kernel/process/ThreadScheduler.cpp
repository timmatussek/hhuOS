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

    dispatch(*currentThread, *getNextThread());
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

void ThreadScheduler::yield() {

    if (!isThreadWaiting()) {

        ProcessScheduler::getInstance().yield();
    }

    yield(*currentThread);
}

void ThreadScheduler::yield(Thread &oldThread) {

    dispatch(oldThread, *getNextThread());
}

void ThreadScheduler::block() {

    dispatch(*currentThread, *getNextThread());
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

Thread *ThreadScheduler::getNextThread() {

    if (!isThreadWaiting()) {
        ProcessScheduler::getInstance().exit();
    }

    lock.acquire();

    Util::ArrayBlockingQueue<Thread *> *currentQueue = &readyQueues[priority.getNextPriority()];

    while (currentQueue->isEmpty()) {
        currentQueue = &readyQueues[priority.getNextPriority()];
    }

    Thread *ret = currentQueue->pop();

    currentQueue->push(ret);

    lock.release();

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