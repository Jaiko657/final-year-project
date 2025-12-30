#include "unity.h"

#include "modules/prefab/prefab_cmp.h"

static prefab_component_t make_comp(const char* type_name, prefab_kv_t* props, size_t prop_count)
{
    return (prefab_component_t){
        .id = ENUM_COMPONENT_COUNT,
        .type_name = (char*)type_name,
        .props = props,
        .prop_count = prop_count,
        .anim = NULL,
        .override_after_spawn = false,
    };
}

void test_prefab_cmp_follow_build_parses_target_player_and_vision(void)
{
    prefab_kv_t props[] = {
        { .name = "target", .value = "player" },
        { .name = "desired_distance", .value = "10" },
        { .name = "max_speed", .value = "20" },
        { .name = "vision_range", .value = "30" },
    };
    prefab_component_t comp = make_comp("FOLLOW", props, 4);

    prefab_cmp_follow_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_follow_build(&comp, NULL, &out));
    TEST_ASSERT_EQUAL_INT(PREFAB_FOLLOW_TARGET_PLAYER, (int)out.target_kind);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, out.desired_distance);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, out.max_speed);
    TEST_ASSERT_TRUE(out.have_vision_range);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, out.vision_range);
}

void test_prefab_cmp_follow_build_parses_numeric_target(void)
{
    prefab_kv_t props[] = {
        { .name = "target", .value = "42" },
        { .name = "desired_distance", .value = "1" },
        { .name = "max_speed", .value = "2" },
    };
    prefab_component_t comp = make_comp("FOLLOW", props, 3);

    prefab_cmp_follow_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_follow_build(&comp, NULL, &out));
    TEST_ASSERT_EQUAL_INT(PREFAB_FOLLOW_TARGET_ENTITY_IDX, (int)out.target_kind);
    TEST_ASSERT_EQUAL_UINT32(42u, out.target_idx);
}

