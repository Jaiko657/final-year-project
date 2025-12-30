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

void test_prefab_cmp_door_build_parses_tiles_string(void)
{
    prefab_kv_t props[] = {
        { .name = "proximity_radius", .value = "12" },
        { .name = "door_tiles", .value = "1,2;3,4" },
    };
    prefab_component_t comp = make_comp("DOOR", props, 2);

    prefab_cmp_door_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_door_build(&comp, NULL, &out));
    TEST_ASSERT_EQUAL_FLOAT(12.0f, out.prox_radius);
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)out.tiles.size);
    TEST_ASSERT_EQUAL_INT(1, out.tiles.data[0].x);
    TEST_ASSERT_EQUAL_INT(2, out.tiles.data[0].y);
    TEST_ASSERT_EQUAL_INT(3, out.tiles.data[1].x);
    TEST_ASSERT_EQUAL_INT(4, out.tiles.data[1].y);

    prefab_cmp_door_free(&out);
}

