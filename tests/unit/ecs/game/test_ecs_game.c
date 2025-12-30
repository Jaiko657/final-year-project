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

void test_cmp_add_inventory_item_vendor_and_getters(void)
{
    ecs_gen[0] = 1;
    ecs_entity_t e = {0, 1};

    cmp_add_inventory(e);
    cmp_add_item(e, ITEM_COIN);
    cmp_add_vendor(e, ITEM_HAT, 5);

    int kind = 0;
    int coins = 0;
    bool hat = false;
    int sells = 0;
    int price = 0;

    TEST_ASSERT_TRUE(ecs_game_get_item(e, &kind));
    TEST_ASSERT_EQUAL_INT(ITEM_COIN, kind);
    TEST_ASSERT_TRUE(ecs_game_get_inventory(e, &coins, &hat));
    TEST_ASSERT_EQUAL_INT(0, coins);
    TEST_ASSERT_FALSE(hat);
    TEST_ASSERT_TRUE(ecs_game_get_vendor(e, &sells, &price));
    TEST_ASSERT_EQUAL_INT(ITEM_HAT, sells);
    TEST_ASSERT_EQUAL_INT(5, price);
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

void test_game_get_player_stats_reads_inventory(void)
{
    ecs_gen[1] = 1;
    ecs_entity_t player = {1, 1};
    ecs_game_stub_set_player(player);

    cmp_add_inventory(player);
    int idx = ent_index_checked(player);
    TEST_ASSERT_TRUE(idx >= 0);
    // Directly adjust inventory via mask since storage is internal to ecs_game.c
    // Simulate coins/hat by adding vendor and updating sprites? Use getters to set expectations.
    // We can only validate that defaults are returned when not modified.
    int coins = -1;
    bool hat = true;
    game_get_player_stats(&coins, &hat);
    TEST_ASSERT_EQUAL_INT(0, coins);
    TEST_ASSERT_FALSE(hat);
}

void test_game_vendor_hint_returns_false_when_missing_data(void)
{
    int x = 0, y = 0;
    const char* text = NULL;
    TEST_ASSERT_FALSE(game_vendor_hint_is_active(&x, &y, &text));

    ecs_gen[0] = 1;
    ecs_entity_t player = {0, 1};
    ecs_game_stub_set_player(player);
    TEST_ASSERT_FALSE(game_vendor_hint_is_active(&x, &y, &text));
}

void test_game_vendor_hint_is_active_for_nearby_vendor(void)
{
    ecs_gen[0] = 1;
    ecs_entity_t player = {0, 1};
    ecs_game_stub_set_player(player);
    ecs_mask[0] = CMP_POS | CMP_COL | CMP_PLAYER;
    cmp_pos[0] = (cmp_position_t){ 0.0f, 0.0f };
    cmp_col[0] = (cmp_collider_t){ 1.0f, 1.0f };

    ecs_gen[1] = 1;
    ecs_entity_t vendor = {1, 1};
    ecs_mask[1] = CMP_POS | CMP_COL;
    cmp_pos[1] = (cmp_position_t){ 5.0f, 0.0f };
    cmp_col[1] = (cmp_collider_t){ 1.0f, 1.0f };
    cmp_add_vendor(vendor, ITEM_HAT, 7);

    int x = 0, y = 0;
    const char* text = NULL;
    TEST_ASSERT_TRUE(game_vendor_hint_is_active(&x, &y, &text));
    TEST_ASSERT_EQUAL_INT(5, x);
    TEST_ASSERT_NOT_NULL(text);
}

void test_ecs_game_getters_fail_when_missing_components(void)
{
    ecs_gen[0] = 1;
    ecs_entity_t e = {0, 1};
    int out_i = 0;
    bool out_b = false;

    TEST_ASSERT_FALSE(ecs_game_get_item(e, &out_i));
    TEST_ASSERT_FALSE(ecs_game_get_inventory(e, &out_i, &out_b));
    TEST_ASSERT_FALSE(ecs_game_get_vendor(e, &out_i, &out_i));
}

void test_ecs_register_game_systems_registers_callbacks(void)
{
    ecs_register_game_systems();
    TEST_ASSERT_TRUE(g_ecs_register_system_calls > 0);
}

void test_sys_pickups_collects_coin_from_proximity(void)
{
    ecs_gen[0] = 1;
    ecs_gen[1] = 1;
    ecs_entity_t player = {0, 1};
    ecs_entity_t coin = {1, 1};

    cmp_add_inventory(player);
    cmp_add_item(coin, ITEM_COIN);

    ecs_prox_view_t enter = { .trigger_owner = player, .matched_entity = coin };
    ecs_game_stub_set_prox_enter(&enter, 1);

    TEST_ASSERT_NOT_NULL(g_ecs_sys_pickups);
    g_ecs_sys_pickups(0.0f, NULL);

    int coins = -1;
    bool has_hat = true;
    TEST_ASSERT_TRUE(ecs_game_get_inventory(player, &coins, &has_hat));
    TEST_ASSERT_EQUAL_INT(1, coins);
    TEST_ASSERT_FALSE(has_hat);
    TEST_ASSERT_EQUAL_UINT32(0u, ecs_gen[coin.idx]);
    TEST_ASSERT_TRUE(g_ui_toast_calls > 0);
}

void test_sys_interact_buys_hat_and_sets_follow(void)
{
    ecs_gen[0] = 1;
    ecs_gen[1] = 1;
    ecs_entity_t player = {0, 1};
    ecs_entity_t vendor = {1, 1};
    ecs_game_stub_set_player(player);

    cmp_add_inventory(player);
    ecs_mask[player.idx] |= (CMP_POS | CMP_COL | CMP_SPR | CMP_PLAYER);
    cmp_pos[player.idx] = (cmp_position_t){ 0.0f, 0.0f };
    cmp_col[player.idx] = (cmp_collider_t){ 1.0f, 1.0f };
    cmp_spr[player.idx].tex = (tex_handle_t){2, 2};

    ecs_gen[2] = 1;
    ecs_gen[3] = 1;
    ecs_gen[4] = 1;
    ecs_entity_t coin0 = {2, 1};
    ecs_entity_t coin1 = {3, 1};
    ecs_entity_t coin2 = {4, 1};
    cmp_add_item(coin0, ITEM_COIN);
    cmp_add_item(coin1, ITEM_COIN);
    cmp_add_item(coin2, ITEM_COIN);
    ecs_prox_view_t enters[3] = {
        { .trigger_owner = player, .matched_entity = coin0 },
        { .trigger_owner = player, .matched_entity = coin1 },
        { .trigger_owner = player, .matched_entity = coin2 },
    };
    ecs_game_stub_set_prox_enter(enters, 3);
    TEST_ASSERT_NOT_NULL(g_ecs_sys_pickups);
    g_ecs_sys_pickups(0.0f, NULL);

    cmp_add_vendor(vendor, ITEM_HAT, 3);
    ecs_mask[vendor.idx] |= (CMP_POS | CMP_COL | CMP_SPR | CMP_BILLBOARD);
    cmp_pos[vendor.idx] = (cmp_position_t){ 2.0f, 0.0f };
    cmp_col[vendor.idx] = (cmp_collider_t){ 1.0f, 1.0f };
    cmp_spr[vendor.idx].src = rectf_xywh(0.0f, 0.0f, 16.0f, 16.0f);
    cmp_billboard[vendor.idx].state = BILLBOARD_ACTIVE;

    ecs_prox_view_t stay = { .trigger_owner = vendor, .matched_entity = player };
    ecs_game_stub_set_prox_stay(&stay, 1);

    input_t in = {0};
    in.pressed = (1ull << BTN_INTERACT);

    TEST_ASSERT_NOT_NULL(g_ecs_sys_interact);
    g_ecs_sys_interact(0.0f, &in);

    TEST_ASSERT_TRUE(g_ui_toast_calls > 0);
    TEST_ASSERT_EQUAL_INT(1, g_asset_release_calls);
    TEST_ASSERT_EQUAL_INT(1, g_asset_acquire_calls);
    TEST_ASSERT_EQUAL_STRING("assets/images/player_hat.png", g_asset_last_path);
    TEST_ASSERT_EQUAL_INT(1, g_cmp_add_follow_calls);
    TEST_ASSERT_EQUAL_UINT32(player.idx, g_cmp_add_follow_last_target.idx);
    TEST_ASSERT_EQUAL_INT(BILLBOARD_INACTIVE, cmp_billboard[vendor.idx].state);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 16.0f, cmp_spr[vendor.idx].src.y);
}

void test_sys_interact_fails_when_insufficient_coins(void)
{
    ecs_gen[0] = 1;
    ecs_gen[1] = 1;
    ecs_entity_t player = {0, 1};
    ecs_entity_t vendor = {1, 1};
    ecs_game_stub_set_player(player);

    cmp_add_inventory(player);
    ecs_mask[player.idx] |= (CMP_POS | CMP_COL | CMP_PLAYER);
    cmp_pos[player.idx] = (cmp_position_t){ 0.0f, 0.0f };
    cmp_col[player.idx] = (cmp_collider_t){ 1.0f, 1.0f };

    cmp_add_vendor(vendor, ITEM_HAT, 3);
    ecs_mask[vendor.idx] |= (CMP_POS | CMP_COL);
    cmp_pos[vendor.idx] = (cmp_position_t){ 2.0f, 0.0f };
    cmp_col[vendor.idx] = (cmp_collider_t){ 1.0f, 1.0f };

    ecs_prox_view_t stay = { .trigger_owner = vendor, .matched_entity = player };
    ecs_game_stub_set_prox_stay(&stay, 1);

    input_t in = {0};
    in.pressed = (1ull << BTN_INTERACT);

    TEST_ASSERT_NOT_NULL(g_ecs_sys_interact);
    g_ecs_sys_interact(0.0f, &in);

    TEST_ASSERT_TRUE(g_ui_toast_calls > 0);
    TEST_ASSERT_EQUAL_INT(0, g_asset_acquire_calls);
    TEST_ASSERT_EQUAL_INT(0, g_cmp_add_follow_calls);
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
