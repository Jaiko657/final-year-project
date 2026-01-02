#include "unity.h"

#include "modules/ecs/ecs.h"
#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs_doors.h"
#include "modules/ecs/ecs_render.h"
#include "ecs_core_stubs.h"
#include "modules/core/input.h"
#include "modules/systems/systems_registration.h"

void setUp(void)
{
    ecs_core_stub_reset();
    ecs_init();
    systems_registration_init();
}

void tearDown(void)
{
    ecs_shutdown();
}

void test_ecs_create_destroy_reuses_slot_and_gen(void)
{
    ecs_entity_t e1 = ecs_create();
    TEST_ASSERT_TRUE(ecs_alive_handle(e1));
    TEST_ASSERT_EQUAL_UINT32(0u, e1.idx);
    TEST_ASSERT_EQUAL_UINT32(1u, e1.gen);

    ecs_destroy(e1);
    TEST_ASSERT_FALSE(ecs_alive_handle(e1));

    ecs_entity_t e2 = ecs_create();
    TEST_ASSERT_EQUAL_UINT32(e1.idx, e2.idx);
    TEST_ASSERT_TRUE(e2.gen != e1.gen);
}

void test_ecs_init_registers_systems_and_tick_runs_phases(void)
{
#if DEBUG_BUILD
    const int expected_registers = 24;
#else
    const int expected_registers = 23;
#endif
    TEST_ASSERT_EQUAL_INT(expected_registers, g_ecs_register_system_calls);

    input_t in = {0};
    systems_tick(0.016f, &in);

    TEST_ASSERT_EQUAL_INT(1, g_ecs_phase_calls[PHASE_INPUT]);
    TEST_ASSERT_EQUAL_INT(1, g_ecs_phase_calls[PHASE_SIM_PRE]);
    TEST_ASSERT_EQUAL_INT(1, g_ecs_phase_calls[PHASE_PHYSICS]);
    TEST_ASSERT_EQUAL_INT(1, g_ecs_phase_calls[PHASE_SIM_POST]);
    TEST_ASSERT_EQUAL_INT(1, g_ecs_phase_calls[PHASE_DEBUG]);

    TEST_ASSERT_EQUAL_INT(5, g_ecs_phase_order_count);
    TEST_ASSERT_EQUAL_INT(PHASE_INPUT, g_ecs_phase_order[0]);
    TEST_ASSERT_EQUAL_INT(PHASE_SIM_PRE, g_ecs_phase_order[1]);
    TEST_ASSERT_EQUAL_INT(PHASE_PHYSICS, g_ecs_phase_order[2]);
    TEST_ASSERT_EQUAL_INT(PHASE_SIM_POST, g_ecs_phase_order[3]);
    TEST_ASSERT_EQUAL_INT(PHASE_DEBUG, g_ecs_phase_order[4]);

    systems_present(0.1f);
    TEST_ASSERT_EQUAL_INT(1, g_ecs_phase_calls[PHASE_PRESENT]);
    TEST_ASSERT_EQUAL_INT(1, g_ecs_phase_calls[PHASE_RENDER]);
}

void test_ecs_get_player_position_and_get_position(void)
{
    float x = -1.0f;
    float y = -1.0f;
    TEST_ASSERT_FALSE(ecs_get_player_position(&x, &y));

    ecs_entity_t player = ecs_create();
    cmp_add_player(player);
    TEST_ASSERT_FALSE(ecs_get_player_position(&x, &y));

    cmp_add_position(player, 3.0f, 4.0f);
    TEST_ASSERT_TRUE(ecs_get_player_position(&x, &y));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, y);

    v2f pos = {0};
    TEST_ASSERT_TRUE(ecs_get_position(player, &pos));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, pos.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, pos.y);

    ecs_entity_t other = ecs_create();
    TEST_ASSERT_FALSE(ecs_get_position(other, &pos));
}

void test_cmp_add_follow_sets_last_seen_when_target_has_pos(void)
{
    ecs_entity_t target = ecs_create();
    cmp_add_position(target, 10.0f, 20.0f);

    ecs_entity_t follower = ecs_create();
    cmp_add_follow(follower, target, 5.0f, 7.0f);

    int idx = ent_index_checked(follower);
    TEST_ASSERT_TRUE(idx >= 0);
    TEST_ASSERT_TRUE(cmp_follow[idx].has_last_seen);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, cmp_follow[idx].last_seen_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, cmp_follow[idx].last_seen_y);

    ecs_entity_t no_pos = ecs_create();
    ecs_entity_t follower2 = ecs_create();
    cmp_add_follow(follower2, no_pos, 2.0f, 3.0f);

    int idx2 = ent_index_checked(follower2);
    TEST_ASSERT_TRUE(idx2 >= 0);
    TEST_ASSERT_FALSE(cmp_follow[idx2].has_last_seen);
}

void test_cmp_add_billboard_warns_without_trigger(void)
{
    ecs_entity_t e = ecs_create();
    cmp_add_billboard(e, "Hello", 1.0f, 2.0f, BILLBOARD_ACTIVE);

    int idx = ent_index_checked(e);
    TEST_ASSERT_TRUE(idx >= 0);
    TEST_ASSERT_TRUE(ecs_mask[idx] & CMP_BILLBOARD);
    TEST_ASSERT_EQUAL_INT(1, g_log_warn_calls);
}

void test_cmp_add_grav_gun_initializes_empty(void)
{
    ecs_entity_t e = ecs_create();
    cmp_add_grav_gun(e);

    int idx = ent_index_checked(e);
    TEST_ASSERT_TRUE(idx >= 0);
    TEST_ASSERT_EQUAL_INT(GRAV_GUN_STATE_FREE, cmp_grav_gun[idx].state);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmp_grav_gun[idx].pickup_distance);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmp_grav_gun[idx].pickup_radius);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmp_grav_gun[idx].max_hold_distance);
}

void test_ecs_destroy_releases_resources(void)
{
    ecs_entity_t e = ecs_create();
    cmp_add_position(e, 1.0f, 2.0f);
    cmp_add_size(e, 3.0f, 4.0f);
    cmp_add_phys_body(e, PHYS_DYNAMIC, 2.0f);
    cmp_add_sprite_handle(e, (tex_handle_t){2, 3}, rectf_xywh(0.0f, 0.0f, 1.0f, 1.0f), 0.0f, 0.0f);
    door_tile_xy_t tiles[1] = { {1, 1} };
    cmp_add_door(e, 5.0f, 1, tiles);

    ecs_destroy(e);

    TEST_ASSERT_EQUAL_INT(1, g_phys_destroy_calls);
    TEST_ASSERT_EQUAL_INT(1, g_asset_release_calls);
    TEST_ASSERT_EQUAL_INT(1, g_world_door_unregister_calls);
}

void test_ecs_mark_cleanup_and_destroy_pipeline(void)
{
    ecs_entity_t e = ecs_create();
    cmp_add_position(e, 1.0f, 2.0f);
    cmp_add_sprite_handle(e, (tex_handle_t){2, 3}, rectf_xywh(0.0f, 0.0f, 1.0f, 1.0f), 0.0f, 0.0f);

    ecs_mark_destroy(e);
    ecs_cleanup_marked();

    TEST_ASSERT_EQUAL_INT(1, g_asset_release_calls);
    TEST_ASSERT_TRUE(ecs_alive_handle(e));

    ecs_destroy_marked();

    TEST_ASSERT_EQUAL_UINT32(0u, ecs_gen[e.idx]);
    TEST_ASSERT_EQUAL_INT(1, g_asset_release_calls);
}

void test_ecs_count_entities_counts_masks(void)
{
    ecs_entity_t a = ecs_create();
    cmp_add_position(a, 1.0f, 1.0f);

    ecs_entity_t b = ecs_create();
    cmp_add_position(b, 2.0f, 2.0f);
    cmp_add_velocity(b, 0.0f, 0.0f, DIR_SOUTH);

    uint32_t masks[2] = { CMP_POS, (CMP_POS | CMP_VEL) };
    ecs_count_result_t result = ecs_count_entities(masks, 2);

    TEST_ASSERT_EQUAL_INT(2, result.count[0]);
    TEST_ASSERT_EQUAL_INT(1, result.count[1]);
}

void test_cmp_add_phys_body_creates_when_requirements_met(void)
{
    ecs_entity_t e = ecs_create();
    cmp_add_position(e, 1.0f, 2.0f);
    cmp_add_size(e, 3.0f, 4.0f);
    cmp_add_phys_body(e, PHYS_DYNAMIC, 2.0f);

    int idx = ent_index_checked(e);
    TEST_ASSERT_TRUE(idx >= 0);
    TEST_ASSERT_TRUE((ecs_mask[idx] & (CMP_POS | CMP_COL | CMP_PHYS_BODY)) == (CMP_POS | CMP_COL | CMP_PHYS_BODY));
    TEST_ASSERT_TRUE(cmp_phys_body[idx].created);
    TEST_ASSERT_EQUAL_INT(1, g_phys_create_calls);
}

void test_cmp_add_sprite_path_addrefs_and_releases(void)
{
    ecs_entity_t e = ecs_create();
    cmp_add_sprite_path(e, "dummy.png", rectf_xywh(0.0f, 0.0f, 1.0f, 1.0f), 0.5f, 0.5f);

    TEST_ASSERT_EQUAL_INT(1, g_asset_acquire_calls);
    TEST_ASSERT_EQUAL_INT(1, g_asset_addref_calls);
    TEST_ASSERT_EQUAL_INT(1, g_asset_release_calls);
}

void test_cmp_add_door_registers_and_unregisters(void)
{
    ecs_entity_t e = ecs_create();
    door_tile_xy_t tiles[2] = { {1,2}, {3,4} };
    cmp_add_door(e, 10.0f, 2, tiles);

    TEST_ASSERT_EQUAL_INT(1, g_world_door_register_calls);
    TEST_ASSERT_EQUAL_INT(2, g_world_door_last_count);

    cmp_add_door(e, 5.0f, 0, NULL);
    TEST_ASSERT_EQUAL_INT(1, g_world_door_unregister_calls);
}

void test_helpers_validate_handles_and_clampf(void)
{
    ecs_entity_t e = ecs_create();
    TEST_ASSERT_TRUE(ecs_alive_idx((int)e.idx));
    TEST_ASSERT_EQUAL_INT((int)e.idx, ent_index_checked(e));

    ecs_entity_t wrong_gen = { e.idx, e.gen + 1 };
    TEST_ASSERT_EQUAL_INT(-1, ent_index_checked(wrong_gen));
    TEST_ASSERT_FALSE(ecs_alive_handle(wrong_gen));

    ecs_entity_t handle = handle_from_index((int)e.idx);
    TEST_ASSERT_EQUAL_UINT32(e.idx, handle.idx);
    TEST_ASSERT_EQUAL_UINT32(e.gen, handle.gen);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, clampf(2.0f, 1.0f, 3.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, clampf(-2.0f, 1.0f, 3.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, clampf(5.0f, 1.0f, 3.0f));
}

void test_cmp_add_trigger_and_phys_body(void)
{
    ecs_entity_t e = ecs_create();
    cmp_add_trigger(e, 2.5f, CMP_PLAYER);

    int idx = ent_index_checked(e);
    TEST_ASSERT_TRUE(idx >= 0);
    TEST_ASSERT_TRUE(ecs_mask[idx] & CMP_TRIGGER);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, cmp_trigger[idx].pad);
    TEST_ASSERT_EQUAL_UINT32(CMP_PLAYER, cmp_trigger[idx].target_mask);

    cmp_add_position(e, 1.0f, 1.0f);
    cmp_add_size(e, 2.0f, 3.0f);
    cmp_add_phys_body(e, PHYS_DYNAMIC, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, cmp_phys_body[idx].mass);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, cmp_phys_body[idx].inv_mass);
}

void test_cmp_add_size_triggers_create_after_phys_body(void)
{
    ecs_entity_t e = ecs_create();
    cmp_add_phys_body(e, PHYS_DYNAMIC, 2.0f);
    cmp_add_position(e, 1.0f, 2.0f);
    cmp_add_size(e, 3.0f, 4.0f);

    TEST_ASSERT_EQUAL_INT(1, g_phys_create_calls);
    int idx = ent_index_checked(e);
    TEST_ASSERT_TRUE(idx >= 0);
    TEST_ASSERT_TRUE(cmp_phys_body[idx].created);
}

void test_cmp_add_sprite_handle_skips_invalid_addref(void)
{
    ecs_entity_t e = ecs_create();
    g_asset_valid_result = false;
    cmp_add_sprite_handle(e, (tex_handle_t){ 1, 1 }, rectf_xywh(0.0f, 0.0f, 1.0f, 1.0f), 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT(0, g_asset_addref_calls);
}
