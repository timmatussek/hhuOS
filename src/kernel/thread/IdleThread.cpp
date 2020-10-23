#include <kernel/core/System.h>
#include "IdleThread.h"

namespace Kernel {

IdleThread::IdleThread() : KernelThread(Kernel::System::getKernelProcess(), "IdleThread", 0) {

}

void IdleThread::run() {
    while (isRunning) {
        yield();
    }
}

}