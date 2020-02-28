#ifndef HHUOS_SIMPLETHREAD_H
#define HHUOS_SIMPLETHREAD_H

#include "kernel/thread/KernelThread.h"
#include "kernel/thread/UserThread.h"

template <typename  T>
class SimpleThread : public T {

public:

    explicit SimpleThread(void (*work)()) noexcept;

    void run() override;

private:

    void (*work)();
};

template class SimpleThread<Kernel::KernelThread>;
// template class SimpleThread<Kernel::UserThread>;

#endif
