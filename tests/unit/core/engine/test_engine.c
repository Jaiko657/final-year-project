#include "unity.h"

#include <string.h>

#include "modules/core/engine.h"
#include "engine_stubs.h"

void setUp(void)
{
    engine_stub_reset();
}

void tearDown(void)
{
}

void test_engine_init_success_calls_subsystems(void)
{
    g_world_load_results[0] = true;
    g_world_load_result_count = 1;
    g_renderer_init_result = true;
    g_renderer_bind_result = true;
    g_init_entities_result = true;

    g_world_px_w = 320;
    g_world_px_h = 240;
    g_camera_view.center = (v2f){ 5.0f, 6.0f };

    TEST_ASSERT_TRUE(engine_init("UnitTest"));

    TEST_ASSERT_EQUAL_INT(1, g_platform_init_calls);
    TEST_ASSERT_EQUAL_INT(1, g_logger_use_raylib_calls);
    TEST_ASSERT_EQUAL_INT(1, g_log_set_min_level_calls);
    TEST_ASSERT_EQUAL_INT(1, g_ui_toast_init_calls);
    TEST_ASSERT_EQUAL_INT(1, g_input_init_calls);
    TEST_ASSERT_EQUAL_INT(1, g_asset_init_calls);
    TEST_ASSERT_EQUAL_INT(1, g_ecs_init_calls);
    TEST_ASSERT_EQUAL_INT(1, g_ecs_register_game_systems_calls);
    TEST_ASSERT_EQUAL_INT(1, g_systems_registration_init_calls);
    TEST_ASSERT_EQUAL_INT(1, g_world_load_from_tmx_calls);
    TEST_ASSERT_EQUAL_INT(1, g_camera_init_calls);
    TEST_ASSERT_EQUAL_INT(1, g_renderer_init_calls);
    TEST_ASSERT_EQUAL_INT(1, g_renderer_bind_calls);
    TEST_ASSERT_EQUAL_INT(1, g_init_entities_calls);

    TEST_ASSERT_EQUAL_INT(1280, g_renderer_init_width);
    TEST_ASSERT_EQUAL_INT(720, g_renderer_init_height);
    TEST_ASSERT_EQUAL_INT(0, g_renderer_init_fps);
    TEST_ASSERT_EQUAL_STRING("UnitTest", g_renderer_init_title);

    TEST_ASSERT_TRUE(g_camera_set_config_calls >= 2);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, g_camera_cfg.zoom);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 16.0f, g_camera_cfg.deadzone_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 16.0f, g_camera_cfg.deadzone_y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 320.0f, g_camera_cfg.bounds.w);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 240.0f, g_camera_cfg.bounds.h);
}

void test_engine_init_failure_cleans_up(void)
{
    g_world_load_results[0] = false;
    g_world_load_result_count = 1;

    TEST_ASSERT_FALSE(engine_init("Fail"));

    TEST_ASSERT_EQUAL_INT(1, g_ecs_shutdown_calls);
    TEST_ASSERT_EQUAL_INT(1, g_asset_shutdown_calls);
    TEST_ASSERT_EQUAL_INT(1, g_renderer_shutdown_calls);
    TEST_ASSERT_EQUAL_INT(1, g_camera_shutdown_calls);
    TEST_ASSERT_EQUAL_INT(1, g_world_shutdown_calls);
    TEST_ASSERT_EQUAL_INT(0, g_init_entities_calls);
}

void test_engine_reload_world_reverts_on_bind_failure(void)
{
    g_world_load_results[0] = true;  // load new path
    g_world_load_results[1] = true;  // revert
    g_world_load_result_count = 2;
    g_renderer_bind_result = false;

    TEST_ASSERT_FALSE(engine_reload_world_from_path("custom.tmx"));
    TEST_ASSERT_EQUAL_INT(2, g_world_load_from_tmx_calls);
    TEST_ASSERT_EQUAL_STRING("assets/maps/start.tmx", g_world_load_last_path);
}

void test_engine_init_renderer_init_failure_cleans_up(void)
{
    g_world_load_results[0] = true;
    g_world_load_result_count = 1;
    g_renderer_init_result = false;

    TEST_ASSERT_FALSE(engine_init("FailRendererInit"));
    TEST_ASSERT_EQUAL_INT(1, g_renderer_init_calls);
    TEST_ASSERT_EQUAL_INT(1, g_renderer_shutdown_calls);
    TEST_ASSERT_EQUAL_INT(1, g_camera_shutdown_calls);
    TEST_ASSERT_EQUAL_INT(1, g_world_shutdown_calls);
}

void test_engine_init_renderer_bind_failure_cleans_up(void)
{
    g_world_load_results[0] = true;
    g_world_load_result_count = 1;
    g_renderer_init_result = true;
    g_renderer_bind_result = false;

    TEST_ASSERT_FALSE(engine_init("FailBind"));
    TEST_ASSERT_EQUAL_INT(1, g_renderer_bind_calls);
    TEST_ASSERT_EQUAL_INT(1, g_renderer_shutdown_calls);
    TEST_ASSERT_EQUAL_INT(1, g_camera_shutdown_calls);
    TEST_ASSERT_EQUAL_INT(1, g_world_shutdown_calls);
}

void test_engine_init_entities_failure_cleans_up(void)
{
    g_world_load_results[0] = true;
    g_world_load_result_count = 1;
    g_renderer_init_result = true;
    g_renderer_bind_result = true;
    g_init_entities_result = false;

    TEST_ASSERT_FALSE(engine_init("FailEntities"));
    TEST_ASSERT_EQUAL_INT(1, g_init_entities_calls);
    TEST_ASSERT_EQUAL_INT(1, g_renderer_shutdown_calls);
    TEST_ASSERT_EQUAL_INT(1, g_camera_shutdown_calls);
    TEST_ASSERT_EQUAL_INT(1, g_world_shutdown_calls);
}

void test_engine_reload_world_success_clamps_camera_to_bounds(void)
{
    g_world_load_results[0] = true;
    g_world_load_result_count = 1;
    g_renderer_bind_result = true;
    g_world_px_w = 100;
    g_world_px_h = 50;
    g_camera_view.center = (v2f){ 120.0f, -5.0f };

    TEST_ASSERT_TRUE(engine_reload_world_from_path("custom.tmx"));
    TEST_ASSERT_EQUAL_STRING("custom.tmx", g_world_load_last_path);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, g_camera_cfg.position.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, g_camera_cfg.position.y);
}

void test_engine_run_processes_one_frame(void)
{
    g_platform_should_close_after = 1;
    g_time_frame_dt = 0.5f;

    TEST_ASSERT_EQUAL_INT(0, engine_run());
    TEST_ASSERT_EQUAL_INT(1, g_input_begin_frame_calls);
    TEST_ASSERT_TRUE(g_systems_tick_calls > 0);
    TEST_ASSERT_EQUAL_INT(g_systems_tick_calls, g_input_for_tick_calls);
    TEST_ASSERT_EQUAL_INT(1, g_systems_present_calls);
}
