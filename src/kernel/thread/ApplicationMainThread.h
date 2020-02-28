#ifndef HHUOS_APPLICATIONMAINTHREAD_H
#define HHUOS_APPLICATIONMAINTHREAD_H

#include "UserThread.h"

namespace Kernel {

class ApplicationMainThread : public UserThread {

public:

    ApplicationMainThread(Process &process, int (*main)(int, char**), int argc, char **argv);

    void run() override;

private:

    int (*main)(int, char**);
    int argc;
    char **argv;
};

}

#endif
