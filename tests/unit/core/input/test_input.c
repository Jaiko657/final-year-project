#include "unity.h"

#include <math.h>

#include "modules/core/input.h"
#include "raylib.h"

static void reset_input_state(void)
{
    raylib_stub_reset();
    input_init_defaults();
}

void setUp(void)
{
    reset_input_state();
}

void tearDown(void)
{
}

void test_input_move_axis_normalizes(void)
{
    raylib_stub_set_key_down(KEY_RIGHT, true);
    raylib_stub_set_key_down(KEY_UP, true);

    input_begin_frame();
    input_t in = input_for_tick();

    TEST_ASSERT_TRUE(input_down(&in, BTN_RIGHT));
    TEST_ASSERT_TRUE(input_down(&in, BTN_UP));

    float len = sqrtf(in.moveX * in.moveX + in.moveY * in.moveY);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, len);
    TEST_ASSERT_TRUE(in.moveX > 0.0f);
    TEST_ASSERT_TRUE(in.moveY < 0.0f);
}

void test_input_pressed_edges_are_latched_once(void)
{
    raylib_stub_set_key_down(KEY_E, true);
    raylib_stub_set_key_pressed(KEY_E, true);

    input_begin_frame();

    input_t first = input_for_tick();
    TEST_ASSERT_TRUE(input_pressed(&first, BTN_INTERACT));

    input_t second = input_for_tick();
    TEST_ASSERT_FALSE(input_pressed(&second, BTN_INTERACT));
}

void test_input_mouse_wheel_is_latched_once(void)
{
    raylib_stub_set_mouse_wheel(2.0f);
    input_begin_frame();

    input_t first = input_for_tick();
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.0f, first.mouse_wheel);

    input_t second = input_for_tick();
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, second.mouse_wheel);
}

void test_input_bind_adds_custom_key(void)
{
    input_bind(BTN_LEFT, KEY_J);
    raylib_stub_set_key_down(KEY_J, true);

    input_begin_frame();
    input_t in = input_for_tick();

    TEST_ASSERT_TRUE(input_down(&in, BTN_LEFT));
    TEST_ASSERT_TRUE(in.moveX < 0.0f);
}

void test_input_mouse_buttons_and_position(void)
{
    raylib_stub_set_mouse_down(MOUSE_LEFT_BUTTON, true);
    raylib_stub_set_mouse_pos(10.0f, 20.0f);

    input_begin_frame();
    input_t in = input_for_tick();

    TEST_ASSERT_TRUE(input_down(&in, BTN_MOUSE_L));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f, in.mouse.x);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 20.0f, in.mouse.y);
}
