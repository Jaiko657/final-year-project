#include "modules/core/time.h"

#include "raylib.h"

double time_now(void)
{
    return GetTime();
}

float time_frame_dt(void)
{
    return GetFrameTime();
}
