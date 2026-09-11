#include <bits/types/clockid_t.h>
#include <cstdint>
#include <ctime>

#define CLOCK_REALTIME 0  // wall time, the Unix timestamp
#define CLOCK_MONOTONIC 1 // monotonic time

// struct timer_timespec {
//     time_t tv_sec; /* Seconds */
//     long tv_nsec;  /* Nanoseconds [0, 999'999'999] */
// };

static auto get_monotonic_msec() -> uint64_t {
    timespec tval = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tval);
    return uint64_t(tval.tv_sec) * 1000 + tval.tv_nsec / 1000 / 1000;
}
