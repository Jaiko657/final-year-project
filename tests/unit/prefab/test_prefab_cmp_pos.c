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

static tiled_object_t make_obj(float x, float y, float w, float h, uint32_t gid, tiled_property_t* props, size_t prop_count)
{
    return (tiled_object_t){
        .id = 1,
        .gid = gid,
        .layer_name = NULL,
        .layer_z = 0,
        .name = NULL,
        .x = x,
        .y = y,
        .w = w,
        .h = h,
        .animationtype = NULL,
        .proximity_radius = 0,
        .door_tile_count = 0,
        .door_tiles = {{0}},
        .property_count = prop_count,
        .properties = props,
    };
}

void test_prefab_cmp_pos_build_prefers_object_override_when_both_present(void)
{
    prefab_kv_t props[] = {
        { .name = "x", .value = "1" },
        { .name = "y", .value = "2" },
    };
    prefab_component_t comp = make_comp("POS", props, 2);

    tiled_property_t obj_props[] = {
        { .name = "POS.x", .value = "5" },
        { .name = "POS.y", .value = "6" },
    };
    tiled_object_t obj = make_obj(0, 0, 0, 0, 0, obj_props, 2);

    prefab_cmp_pos_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_pos_build(&comp, &obj, &out));
    TEST_ASSERT_EQUAL_FLOAT(5.0f, out.x);
    TEST_ASSERT_EQUAL_FLOAT(6.0f, out.y);
}

void test_prefab_cmp_pos_build_defaults_to_object_center_and_allows_partial_override(void)
{
    prefab_component_t comp = make_comp("POS", NULL, 0);

    tiled_property_t obj_props[] = {
        { .name = "POS.x", .value = "7" },
    };
    tiled_object_t obj = make_obj(10.0f, 20.0f, 100.0f, 50.0f, 0, obj_props, 1);

    prefab_cmp_pos_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_pos_build(&comp, &obj, &out));
    TEST_ASSERT_EQUAL_FLOAT(7.0f, out.x);
    TEST_ASSERT_EQUAL_FLOAT(45.0f, out.y); // 20 + 50/2
}

void test_prefab_cmp_pos_build_tile_object_offsets_y_by_half_height(void)
{
    prefab_component_t comp = make_comp("POS", NULL, 0);

    tiled_object_t tile_obj = make_obj(0.0f, 0.0f, 16.0f, 32.0f, 123u, NULL, 0);

    prefab_cmp_pos_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_pos_build(&comp, &tile_obj, &out));
    TEST_ASSERT_EQUAL_FLOAT(8.0f, out.x);
    TEST_ASSERT_EQUAL_FLOAT(-16.0f, out.y); // y - h/2 for gid objects
}

