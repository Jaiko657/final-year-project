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

static tiled_object_t make_obj_with_prop(const char* name, const char* value)
{
    static tiled_property_t props[1];
    props[0] = (tiled_property_t){ .name = (char*)name, .value = (char*)value };
    return (tiled_object_t){
        .id = 1,
        .gid = 0,
        .layer_name = NULL,
        .layer_z = 0,
        .name = NULL,
        .x = 0.0f,
        .y = 0.0f,
        .w = 0.0f,
        .h = 0.0f,
        .animationtype = NULL,
        .proximity_radius = 0,
        .door_tile_count = 0,
        .door_tiles = {{0}},
        .property_count = 1,
        .properties = props,
    };
}

void test_prefab_cmp_trigger_build_uses_object_proximity_radius_when_pad_missing(void)
{
    prefab_kv_t props[] = {
        { .name = "target_mask", .value = "ITEM|COL" },
    };
    prefab_component_t comp = make_comp("TRIGGER", props, 1);

    tiled_object_t obj = make_obj_with_prop("proximity_radius", "12.5");

    prefab_cmp_trigger_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_trigger_build(&comp, &obj, &out));
    TEST_ASSERT_EQUAL_FLOAT(12.5f, out.pad);
    TEST_ASSERT_TRUE((out.target_mask & CMP_ITEM) != 0);
    TEST_ASSERT_TRUE((out.target_mask & CMP_COL) != 0);
}

