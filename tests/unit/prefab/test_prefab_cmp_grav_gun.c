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

void test_prefab_cmp_grav_gun_build_sets_present_overrides(void)
{
    prefab_kv_t props[] = {
        { .name = "pickup_distance", .value = "12" },
        { .name = "damping", .value = "0.5" },
    };
    prefab_component_t comp = make_comp("GRAV_GUN", props, 2);

    prefab_cmp_grav_gun_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_grav_gun_build(&comp, NULL, &out));
    TEST_ASSERT_TRUE(out.has_pickup_distance);
    TEST_ASSERT_EQUAL_FLOAT(12.0f, out.pickup_distance);
    TEST_ASSERT_TRUE(out.has_damping);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, out.damping);
}
