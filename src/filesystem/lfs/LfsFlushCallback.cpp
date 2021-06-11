#include "LfsFlushCallback.h"
#include "kernel/service/TimeService.h"
#include "kernel/core/System.h"

LfsFlushCallback::LfsFlushCallback(Lfs* lfs) noexcept : lfs(lfs) {}

void LfsFlushCallback::run() {
    Kernel::TimeService* timeService = Kernel::System::getService<Kernel::TimeService>();
    uint32_t lastTime = timeService->getSystemTime();
    while(true) {
        if(timeService->getSystemTime() - lastTime > FLUSH_INTERVAL) {
            lfs->flush();
            lastTime = timeService->getSystemTime();
        }
    }
}
