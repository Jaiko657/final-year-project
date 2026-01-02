#include "unity.h"

#include <string.h>

#include "debug_hotkeys_stubs.h"
#include "modules/core/input.h"
#include "modules/ecs/ecs_internal.h"

void sys_debug_binds(const input_t* in);

static input_t make_input_pressed(button_t b)
{
    input_t in = {0};
    in.pressed = (1ull << b);
    return in;
}

void setUp(void)
{
    debug_hotkeys_stub_reset();
}

void tearDown(void)
{
}

void test_debug_hotkeys_asset_reload_path(void)
{
    input_t in = make_input_pressed(BTN_ASSET_DEBUG_PRINT);
    sys_debug_binds(&in);

    TEST_ASSERT_EQUAL_INT(1, g_asset_reload_calls);
    TEST_ASSERT_EQUAL_INT(1, g_asset_log_calls);
    TEST_ASSERT_EQUAL_INT(1, g_toast_calls);
    TEST_ASSERT_EQUAL_STRING("Assets reloaded", g_last_toast);
}

void test_debug_hotkeys_renderer_toggles(void)
{
    input_t in = make_input_pressed(BTN_DEBUG_COLLIDER_ECS);
    sys_debug_binds(&in);
    TEST_ASSERT_EQUAL_INT(1, g_renderer_ecs_calls);
    TEST_ASSERT_EQUAL_STRING("ECS colliders: on", g_last_toast);

    in = make_input_pressed(BTN_DEBUG_COLLIDER_PHYSICS);
    sys_debug_binds(&in);
    TEST_ASSERT_EQUAL_INT(1, g_renderer_phys_calls);
    TEST_ASSERT_EQUAL_STRING("Physics colliders: on", g_last_toast);

    in = make_input_pressed(BTN_DEBUG_COLLIDER_STATIC);
    sys_debug_binds(&in);
    TEST_ASSERT_EQUAL_INT(1, g_renderer_static_calls);
    TEST_ASSERT_EQUAL_STRING("Static colliders: on", g_last_toast);

    in = make_input_pressed(BTN_DEBUG_TRIGGERS);
    sys_debug_binds(&in);
    TEST_ASSERT_EQUAL_INT(1, g_renderer_triggers_calls);
    TEST_ASSERT_EQUAL_STRING("Triggers: on", g_last_toast);
}

void test_debug_hotkeys_reload_world_message(void)
{
    g_engine_reload_world_result = true;
    g_world_tiles_w = 10;
    g_world_tiles_h = 12;

    input_t in = make_input_pressed(BTN_DEBUG_RELOAD_TMX);
    sys_debug_binds(&in);

    TEST_ASSERT_EQUAL_INT(1, g_toast_calls);
    TEST_ASSERT_EQUAL_STRING("TMX reload: ok (10x12 tiles)", g_last_toast);
}

void test_debug_hotkeys_inspect_toggle_toast(void)
{
    input_t in = make_input_pressed(BTN_DEBUG_INSPECT);
    sys_debug_binds(&in);

    TEST_ASSERT_EQUAL_INT(1, g_toast_calls);
    TEST_ASSERT_EQUAL_STRING("Inspect mode: on", g_last_toast);
}

void test_debug_hotkeys_fps_toggle(void)
{
    input_t in = make_input_pressed(BTN_DEBUG_FPS);
    sys_debug_binds(&in);

    TEST_ASSERT_EQUAL_INT(1, g_renderer_fps_calls);
    TEST_ASSERT_EQUAL_STRING("FPS overlay: on", g_last_toast);
}

void test_debug_hotkeys_inspect_click_logs_components(void)
{
    int idx = 1;
    g_ecs_alive[idx] = true;
    ecs_mask[idx] = CMP_POS | CMP_COL | CMP_VEL | CMP_PHYS_BODY | CMP_SPR | CMP_ANIM |
                    CMP_PLAYER | CMP_FOLLOW | CMP_STORAGE |
                    CMP_TRIGGER | CMP_BILLBOARD | CMP_GRAV_GUN | CMP_DOOR;

    cmp_pos[idx].x = 5.0f;
    cmp_pos[idx].y = 5.0f;
    cmp_col[idx].hx = 8.0f;
    cmp_col[idx].hy = 8.0f;

    cmp_vel[idx].x = 1.0f;
    cmp_vel[idx].y = -1.0f;
    cmp_vel[idx].facing.rawDir = DIR_NORTH;
    cmp_vel[idx].facing.facingDir = DIR_EAST;
    cmp_vel[idx].facing.candidateDir = DIR_SOUTH;
    cmp_vel[idx].facing.candidateTime = 0.5f;

    cmp_phys_body[idx].type = PHYS_DYNAMIC;
    cmp_phys_body[idx].mass = 1.0f;
    cmp_phys_body[idx].inv_mass = 1.0f;
    cmp_phys_body[idx].created = true;

    cmp_spr[idx].tex.idx = 2;
    cmp_spr[idx].tex.gen = 3;
    cmp_spr[idx].src = rectf_xywh(0.0f, 0.0f, 16.0f, 16.0f);

    cmp_anim[idx].frame_w = 16;
    cmp_anim[idx].frame_h = 16;
    cmp_anim[idx].current_anim = 0;
    cmp_anim[idx].anim_count = 1;
    cmp_anim[idx].frame_index = 0;
    cmp_anim[idx].frame_duration = 0.1f;
    cmp_anim[idx].current_time = 0.0f;

    cmp_follow[idx].target.idx = 2;
    cmp_follow[idx].desired_distance = 10.0f;
    cmp_follow[idx].max_speed = 5.0f;
    cmp_follow[idx].vision_range = 20.0f;

    cmp_trigger[idx].pad = 1.0f;
    cmp_trigger[idx].target_mask = CMP_PLASTIC;

    cmp_billboard[idx].y_offset = 1.0f;
    cmp_billboard[idx].linger = 2.0f;
    cmp_billboard[idx].timer = 0.5f;
    cmp_billboard[idx].state = BILLBOARD_ACTIVE;
    snprintf(cmp_billboard[idx].text, sizeof(cmp_billboard[idx].text), "hello");

    cmp_grav_gun[idx].state = GRAV_GUN_STATE_FREE;
    cmp_grav_gun[idx].holder = (ecs_entity_t){0};
    cmp_grav_gun[idx].follow_gain = 2.0f;
    cmp_grav_gun[idx].max_speed = 50.0f;
    cmp_grav_gun[idx].hold_vel_x = 0.1f;
    cmp_grav_gun[idx].hold_vel_y = 0.2f;

    cmp_door[idx].prox_radius = 5.0f;
    cmp_door[idx].state = DOOR_OPEN;
    cmp_door[idx].anim_time_ms = 0.0f;
    cmp_door[idx].intent_open = true;
    cmp_door[idx].world_handle = 123u;

    g_game_storage_plastic = 2;
    g_game_storage_capacity = 5;

    input_t in = {0};
    in.pressed = (1ull << BTN_DEBUG_INSPECT) | (1ull << BTN_MOUSE_L);
    in.mouse.x = 5.0f;
    in.mouse.y = 5.0f;
    sys_debug_binds(&in);

    TEST_ASSERT_TRUE(g_log_calls > 0);

    input_t toggle_off = make_input_pressed(BTN_DEBUG_INSPECT);
    sys_debug_binds(&toggle_off);
}
