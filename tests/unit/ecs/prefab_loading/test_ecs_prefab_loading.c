#include "unity.h"

#include "modules/ecs/ecs_prefab_loading.h"
#include "ecs_prefab_loading_stubs.h"
#include "modules/ecs/ecs_internal.h"

void setUp(void)
{
    ecs_prefab_loading_stub_reset();
}

void tearDown(void)
{
}

void test_prefab_spawn_entity_adds_default_position(void)
{
    prefab_t prefab = {0};
    ecs_entity_t e = ecs_prefab_spawn_entity(&prefab, NULL);

    TEST_ASSERT_EQUAL_UINT32(0u, e.idx);
    TEST_ASSERT_EQUAL_INT(1, g_cmp_add_position_calls);
}

void test_prefab_spawn_entity_from_path_failure_returns_null(void)
{
    g_prefab_load_result = false;
    ecs_entity_t e = ecs_prefab_spawn_entity_from_path("missing.ent", NULL);

    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, e.idx);
}

void test_prefab_spawn_from_map_resolves_relative_path(void)
{
    tiled_property_t props[1];
    props[0].name = "entityprefab";
    props[0].value = "foo.ent";
    props[0].type = NULL;

    tiled_object_t obj = {0};
    obj.property_count = 1;
    obj.properties = props;

    world_map_t map = {0};
    map.object_count = 1;
    map.objects = &obj;

    size_t spawned = ecs_prefab_spawn_from_map(&map, "assets/maps/start.tmx");

    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)spawned);
    TEST_ASSERT_EQUAL_INT(1, g_prefab_load_calls);
    TEST_ASSERT_EQUAL_STRING("assets/maps/foo.ent", g_prefab_load_path);
}

void test_prefab_spawn_entity_applies_position_overrides(void)
{
    tiled_property_t props[2];
    props[0].name = "POS.x";
    props[0].value = "12.5";
    props[0].type = NULL;
    props[1].name = "POS.y";
    props[1].value = "34.0";
    props[1].type = NULL;

    tiled_object_t obj = {0};
    obj.x = 1.0f;
    obj.y = 2.0f;
    obj.property_count = 2;
    obj.properties = props;

    prefab_t prefab = {0};
    ecs_entity_t e = ecs_prefab_spawn_entity(&prefab, &obj);

    TEST_ASSERT_EQUAL_UINT32(0u, e.idx);
    TEST_ASSERT_EQUAL_INT(1, g_cmp_add_position_calls);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.5f, cmp_pos[0].x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 34.0f, cmp_pos[0].y);
}

void test_prefab_spawn_entity_overrides_collider_dimensions(void)
{
    prefab_component_t comp = {
        .id = ENUM_COL,
        .override_after_spawn = true,
    };
    prefab_t prefab = {
        .components = &comp,
        .component_count = 1,
    };

    tiled_object_t obj = {0};
    obj.w = 10.0f;
    obj.h = 6.0f;

    ecs_prefab_spawn_entity(&prefab, &obj);

    TEST_ASSERT_EQUAL_INT(1, g_cmp_add_size_calls);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, g_cmp_add_size_last_hx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, g_cmp_add_size_last_hy);
}

void test_prefab_spawn_entity_follow_targets_player_with_vision(void)
{
    prefab_component_t comp = { .id = ENUM_FOLLOW };
    prefab_t prefab = {
        .components = &comp,
        .component_count = 1,
    };

    ecs_entity_t player = {2, 1};
    ecs_prefab_loading_stub_set_player(player);

    g_prefab_cmp_follow_result = true;
    g_prefab_cmp_follow_out = (prefab_cmp_follow_t){
        .target_kind = PREFAB_FOLLOW_TARGET_PLAYER,
        .desired_distance = 3.0f,
        .max_speed = 4.0f,
        .have_vision_range = true,
        .vision_range = 9.0f,
    };

    ecs_prefab_spawn_entity(&prefab, NULL);

    TEST_ASSERT_EQUAL_INT(1, g_cmp_add_follow_calls);
    TEST_ASSERT_EQUAL_UINT32(player.idx, g_cmp_add_follow_last_target.idx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 9.0f, cmp_follow[0].vision_range);
}

void test_prefab_spawn_entity_follow_targets_entity_idx(void)
{
    prefab_component_t comp = { .id = ENUM_FOLLOW };
    prefab_t prefab = {
        .components = &comp,
        .component_count = 1,
    };

    g_prefab_cmp_follow_result = true;
    g_prefab_cmp_follow_out = (prefab_cmp_follow_t){
        .target_kind = PREFAB_FOLLOW_TARGET_ENTITY_IDX,
        .target_idx = 4,
        .desired_distance = 5.0f,
        .max_speed = 6.0f,
    };

    ecs_prefab_spawn_entity(&prefab, NULL);

    TEST_ASSERT_EQUAL_INT(1, g_cmp_add_follow_calls);
    TEST_ASSERT_EQUAL_UINT32(4u, g_cmp_add_follow_last_target.idx);
}

void test_prefab_spawn_entity_warns_on_missing_sprite_path(void)
{
    prefab_component_t comp = { .id = ENUM_SPR };
    prefab_t prefab = {
        .components = &comp,
        .component_count = 1,
    };

    ecs_prefab_spawn_entity(&prefab, NULL);

    TEST_ASSERT_EQUAL_INT(1, g_log_warn_calls);
}
