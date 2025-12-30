#include "unity.h"

#include <stdint.h>

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

static tiled_object_t make_obj(float w, float h)
{
    return (tiled_object_t){
        .id = 1,
        .gid = 0,
        .layer_name = NULL,
        .layer_z = 0,
        .name = NULL,
        .x = 0.0f,
        .y = 0.0f,
        .w = w,
        .h = h,
        .animationtype = NULL,
        .proximity_radius = 0,
        .door_tile_count = 0,
        .door_tiles = {{0}},
        .property_count = 0,
        .properties = NULL,
    };
}

void test_prefab_cmp_col_build_uses_object_w_h_when_missing(void)
{
    prefab_component_t comp = make_comp("COL", NULL, 0);
    tiled_object_t obj = make_obj(100.0f, 50.0f);

    prefab_cmp_col_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_col_build(&comp, &obj, &out));
    TEST_ASSERT_EQUAL_FLOAT(50.0f, out.hx);
    TEST_ASSERT_EQUAL_FLOAT(25.0f, out.hy);
}

