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

void test_prefab_cmp_vel_build_parses_xy_and_dir(void)
{
    prefab_kv_t props[] = {
        { .name = "x", .value = "3" },
        { .name = "y", .value = "4" },
        { .name = "dir", .value = "north" },
    };
    prefab_component_t comp = make_comp("VEL", props, 3);

    prefab_cmp_vel_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_vel_build(&comp, NULL, &out));
    TEST_ASSERT_EQUAL_FLOAT(3.0f, out.x);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, out.y);
    TEST_ASSERT_EQUAL_INT(DIR_NORTH, (int)out.dir);
}

void test_prefab_cmp_vel_build_defaults_dir_to_south(void)
{
    prefab_kv_t props[] = {
        { .name = "x", .value = "1" },
        { .name = "y", .value = "2" },
    };
    prefab_component_t comp = make_comp("VEL", props, 2);

    prefab_cmp_vel_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_vel_build(&comp, NULL, &out));
    TEST_ASSERT_EQUAL_INT(DIR_SOUTH, (int)out.dir);
}

