#include "SimpleThread.h"

template<typename T>
SimpleThread<T>::SimpleThread(void (*work)()) noexcept : work(work) {

}

template<typename T>
void SimpleThread<T>::run() {
    work();
}
