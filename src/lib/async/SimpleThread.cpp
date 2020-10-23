#include "SimpleThread.h"
#include "kernel/core/System.h"

template<typename T>
SimpleThread<T>::SimpleThread(void (*work)()) noexcept : T(Kernel::System::getKernelProcess()), work(work) {

}

template<typename T>
void SimpleThread<T>::run() {
    work();
}
