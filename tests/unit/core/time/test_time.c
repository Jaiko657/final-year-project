#include "unity.h"

#include "modules/core/time.h"
#include "raylib.h"

void setUp(void)
{
    raylib_time_stub_reset();
}

void tearDown(void)
{
}

void test_time_now_uses_raylib_time(void)
{
    raylib_time_stub_set_time(12.5);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 12.5f, (float)time_now());
}

void test_time_frame_dt_uses_raylib_frame_time(void)
{
    raylib_time_stub_set_frame(0.016f);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.016f, time_frame_dt());
}
