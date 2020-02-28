#include "ApplicationMainThread.h"

Kernel::ApplicationMainThread::ApplicationMainThread(Process &process, int (*main)(int, char **), int argc, char **argv) : UserThread(process), main(main), argc(argc), argv(argv) {

}

void Kernel::ApplicationMainThread::run() {
    main(argc, argv);
}