#include "modules/core/time.h"

#if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
#include <time.h>
double time_now(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
#else
#include <time.h>
double time_now(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}
#endif

float time_frame_dt(void)
{
    return 1.0f / 60.0f;
}
