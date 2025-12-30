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

void test_prefab_cmp_billboard_build_parses_text_and_state(void)
{
    prefab_kv_t props[] = {
        { .name = "text", .value = "hi" },
        { .name = "state", .value = "inactive" },
        { .name = "y_offset", .value = "3" },
        { .name = "linger", .value = "4" },
    };
    prefab_component_t comp = make_comp("BILLBOARD", props, 4);

    prefab_cmp_billboard_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_billboard_build(&comp, NULL, &out));
    TEST_ASSERT_EQUAL_STRING("hi", out.text);
    TEST_ASSERT_EQUAL_INT(BILLBOARD_INACTIVE, (int)out.state);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, out.y_offset);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, out.linger);
}

