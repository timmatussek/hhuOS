#ifndef LFS_FLUSH_CALLBACK_H
#define LFS_FLUSH_CALLBACK_H

#include "Lfs.h"
#include "kernel/thread/KernelThread.h"

/**
 * Timeout between flushes in ms.
 */
#define FLUSH_INTERVAL 60000

/*
 * A thread to call flush at regular intervals.
 */
class LfsFlushCallback : public Kernel::KernelThread {

public:

    LfsFlushCallback(Lfs* lfs) noexcept;

    void run() override;

private:

    Lfs* lfs;
};

#endif
