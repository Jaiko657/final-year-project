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

void test_prefab_cmp_spr_build_parses_path_src_and_origin(void)
{
    prefab_kv_t props[] = {
        { .name = "path", .value = "tex.png" },
        { .name = "src", .value = "1,2,3,4" },
        { .name = "ox", .value = "5" },
        { .name = "oy", .value = "6" },
    };
    prefab_component_t comp = make_comp("SPR", props, 4);

    prefab_cmp_spr_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_spr_build(&comp, NULL, &out));
    TEST_ASSERT_EQUAL_STRING("tex.png", out.path);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, out.src.x);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, out.src.y);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, out.src.w);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, out.src.h);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, out.ox);
    TEST_ASSERT_EQUAL_FLOAT(6.0f, out.oy);
}

void test_prefab_cmp_spr_build_returns_false_when_missing_path(void)
{
    prefab_component_t comp = make_comp("SPR", NULL, 0);
    prefab_cmp_spr_t out = {0};
    TEST_ASSERT_FALSE(prefab_cmp_spr_build(&comp, NULL, &out));
}

