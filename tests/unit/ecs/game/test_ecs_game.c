#include "unity.h"

#include "modules/ecs/ecs_game.h"
#include "modules/ecs/ecs_internal.h"
#include "ecs_game_stubs.h"
#include "modules/ecs/ecs_render.h"

#include <string.h>

void setUp(void)
{
    ecs_game_stub_reset();
    ecs_register_game_systems();
}

void tearDown(void)
{
}

void test_init_entities_requires_loaded_map(void)
{
    g_world_tiled_map = NULL;
    TEST_ASSERT_FALSE(init_entities("dummy.tmx"));

    world_map_t map = {0};
    g_world_tiled_map = &map;
    TEST_ASSERT_TRUE(init_entities("maps/test.tmx"));
    TEST_ASSERT_EQUAL_INT(1, g_prefab_spawn_calls);
    TEST_ASSERT_EQUAL_STRING("maps/test.tmx", g_prefab_spawn_last_path);
}

void test_ecs_register_game_systems_registers_callbacks(void)
{
    ecs_register_game_systems();
    TEST_ASSERT_TRUE(g_ecs_register_system_calls > 0);
}

void test_sys_storage_deposit_moves_plastic_into_tardas(void)
{
    ecs_gen[0] = 1;
    ecs_gen[1] = 1;
    ecs_entity_t tardas = {0, 1};
    ecs_entity_t plastic = {1, 1};

    cmp_add_storage(tardas, 2);
    cmp_add_plastic(plastic);
    ecs_mask[plastic.idx] |= CMP_GRAV_GUN;
    cmp_grav_gun[plastic.idx].state = GRAV_GUN_STATE_FREE;
    cmp_grav_gun[plastic.idx].just_dropped = true;

    ecs_prox_view_t stay = { .trigger_owner = tardas, .matched_entity = plastic };
    ecs_game_stub_set_prox_stay(&stay, 1);

    ecs_register_game_systems();
    TEST_ASSERT_NOT_NULL(g_ecs_sys_storage);
    g_ecs_sys_storage(0.0f, NULL);

    int plastic_count = 0;
    int capacity = 0;
    TEST_ASSERT_TRUE(game_get_tardas_storage(&plastic_count, &capacity));
    TEST_ASSERT_EQUAL_INT(1, plastic_count);
    TEST_ASSERT_EQUAL_INT(2, capacity);
    TEST_ASSERT_EQUAL_UINT32(0u, ecs_gen[plastic.idx]);
    TEST_ASSERT_TRUE(g_ui_toast_calls > 0);
}

void test_sys_doors_intent_and_present_updates_state(void)
{
    world_map_t map = {0};
    g_world_tiled_map = &map;
    ecs_gen[2] = 1;
    ecs_entity_t door = {2, 1};
    ecs_mask[door.idx] = CMP_DOOR;
    cmp_door[door.idx].state = DOOR_CLOSED;
    cmp_door[door.idx].world_handle = 7u;

    ecs_prox_view_t stay = { .trigger_owner = door, .matched_entity = ecs_null() };
    ecs_game_stub_set_prox_stay(&stay, 1);

    g_world_door_primary_duration = 100;
    TEST_ASSERT_NOT_NULL(g_ecs_sys_doors_tick);
    g_ecs_sys_doors_tick(0.05f, NULL);
    TEST_ASSERT_TRUE(cmp_door[door.idx].intent_open);
    TEST_ASSERT_EQUAL_INT(DOOR_OPENING, cmp_door[door.idx].state);
    TEST_ASSERT_TRUE(g_world_door_last_forward);
    TEST_ASSERT_TRUE(g_world_door_last_time > 0.0f);

    ecs_game_stub_set_prox_stay(NULL, 0);
    cmp_door[door.idx].state = DOOR_OPEN;
    cmp_door[door.idx].intent_open = false;
    g_ecs_sys_doors_tick(0.05f, NULL);
    TEST_ASSERT_EQUAL_INT(DOOR_CLOSING, cmp_door[door.idx].state);
    TEST_ASSERT_FALSE(g_world_door_last_forward);
}
