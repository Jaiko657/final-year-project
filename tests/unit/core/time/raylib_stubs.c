#include "raylib.h"

static double g_time = 0.0;
static float g_frame = 0.0f;

void raylib_time_stub_reset(void)
{
    g_time = 0.0;
    g_frame = 0.0f;
}

void raylib_time_stub_set_time(double t)
{
    g_time = t;
}

void raylib_time_stub_set_frame(float dt)
{
    g_frame = dt;
}

double GetTime(void)
{
    return g_time;
}

float GetFrameTime(void)
{
    return g_frame;
}
