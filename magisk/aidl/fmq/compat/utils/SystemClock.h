/* libfmq uses this only for relative timeouts, so the monotonic clock is the
   right source and no platform dependency is needed. */
#pragma once
#include <time.h>
#include <stdint.h>
namespace android {
inline int64_t elapsedRealtimeNano() {
    struct timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
}  // namespace android
