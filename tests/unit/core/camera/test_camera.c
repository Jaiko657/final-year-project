#include "unity.h"

#include "modules/core/camera.h"
#include "ecs_stubs.h"

void setUp(void)
{
    camera_init();
    ecs_stub_reset();
}

void tearDown(void)
{
}

void test_camera_deadzone_moves_toward_target(void)
{
    ecs_entity_t target = (ecs_entity_t){ .idx = 1, .gen = 1 };
    ecs_stub_set_target(target);
    ecs_stub_set_position(true, v2f_make(100.0f, 0.0f));

    camera_config_t cfg = camera_get_config();
    cfg.target = target;
    cfg.position = v2f_make(0.0f, 0.0f);
    cfg.offset = v2f_make(0.0f, 0.0f);
    cfg.deadzone_x = 10.0f;
    cfg.deadzone_y = 10.0f;
    cfg.bounds = rectf_xywh(0.0f, 0.0f, 0.0f, 0.0f); // no clamp
    camera_set_config(&cfg);

    camera_tick(0.0f);
    camera_view_t v = camera_get_view();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 90.0f, v.center.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, v.center.y);
}

void test_camera_bounds_clamps_center(void)
{
    ecs_entity_t target = (ecs_entity_t){ .idx = 1, .gen = 1 };
    ecs_stub_set_target(target);
    ecs_stub_set_position(true, v2f_make(100.0f, 100.0f));

    camera_config_t cfg = camera_get_config();
    cfg.target = target;
    cfg.position = v2f_make(0.0f, 0.0f);
    cfg.deadzone_x = 0.0f;
    cfg.deadzone_y = 0.0f;
    cfg.bounds = rectf_xywh(0.0f, 0.0f, 50.0f, 25.0f);
    camera_set_config(&cfg);

    camera_tick(0.0f);
    camera_view_t v = camera_get_view();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, v.center.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, v.center.y);
}

void test_camera_set_config_clamps_invalid_values(void)
{
    camera_config_t cfg = camera_get_config();
    cfg.zoom = 0.0f;
    cfg.padding = -1.0f;
    cfg.deadzone_x = -2.0f;
    cfg.deadzone_y = -3.0f;
    camera_set_config(&cfg);

    camera_view_t v = camera_get_view();
    TEST_ASSERT_TRUE(v.zoom > 0.0f);
    TEST_ASSERT_TRUE(v.padding >= 0.0f);
}
