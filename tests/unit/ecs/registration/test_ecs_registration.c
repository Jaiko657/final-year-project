#include "unity.h"

#include "ecs_registration_stubs.h"
#include "modules/systems/systems_registration.h"

static void assert_registration(int idx, systems_phase_t phase, int order, const char* name)
{
    TEST_ASSERT_TRUE(idx < g_systems_registration_call_count);
    TEST_ASSERT_EQUAL_INT(phase, g_systems_registration_calls[idx].phase);
    TEST_ASSERT_EQUAL_INT(order, g_systems_registration_calls[idx].order);
    TEST_ASSERT_EQUAL_STRING(name, g_systems_registration_calls[idx].name);
}

void setUp(void)
{
    ecs_registration_stubs_reset();
}

void tearDown(void)
{
}

void test_systems_registration_orders_and_hooks(void)
{
    systems_registration_init();

    TEST_ASSERT_TRUE(g_systems_init_seq > 0);
    TEST_ASSERT_TRUE(g_ecs_render_hooks_seq > 0);
    TEST_ASSERT_TRUE(g_ecs_physics_hooks_seq > 0);
    TEST_ASSERT_TRUE(g_ecs_game_hooks_seq > 0);
    TEST_ASSERT_TRUE(g_ecs_door_hooks_seq > 0);

    TEST_ASSERT_EQUAL_INT(14, g_systems_registration_call_count);

    TEST_ASSERT_TRUE(g_systems_init_seq < g_ecs_render_hooks_seq);
    TEST_ASSERT_TRUE(g_ecs_render_hooks_seq < g_ecs_physics_hooks_seq);
    TEST_ASSERT_TRUE(g_ecs_physics_hooks_seq < g_systems_registration_calls[0].seq);

    {
        int last_seq = g_systems_registration_calls[g_systems_registration_call_count - 1].seq;
        TEST_ASSERT_TRUE(g_ecs_game_hooks_seq > last_seq);
        TEST_ASSERT_TRUE(g_ecs_door_hooks_seq > g_ecs_game_hooks_seq);
    }

    assert_registration(0, PHASE_INPUT, 0, "input");
    assert_registration(1, PHASE_INPUT, 50, "liftable_input");
    assert_registration(2, PHASE_SIM_PRE, 100, "animation_controller");
    assert_registration(3, PHASE_PHYSICS, 50, "follow_ai");
    assert_registration(4, PHASE_PHYSICS, 90, "liftable_motion");
    assert_registration(5, PHASE_PHYSICS, 100, "physics");
    assert_registration(6, PHASE_SIM_POST, 100, "proximity_view");
    assert_registration(7, PHASE_SIM_POST, 200, "billboards");
    assert_registration(8, PHASE_SIM_POST, 900, "world_apply_edits");
    assert_registration(9, PHASE_PRESENT, 10, "toast_update");
    assert_registration(10, PHASE_PRESENT, 20, "camera_tick");
    assert_registration(11, PHASE_PRESENT, 100, "sprite_anim");
    assert_registration(12, PHASE_PRESENT, 1000, "renderer_present");
    assert_registration(13, PHASE_PRESENT, 1100, "asset_collect");
}
