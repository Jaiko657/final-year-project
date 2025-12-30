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

void test_prefab_cmp_phys_body_build_parses_type_and_mass(void)
{
    prefab_kv_t props[] = {
        { .name = "type", .value = "static" },
        { .name = "mass", .value = "2.5" },
    };
    prefab_component_t comp = make_comp("PHYS_BODY", props, 2);

    prefab_cmp_phys_body_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_phys_body_build(&comp, NULL, &out));
    TEST_ASSERT_EQUAL_INT(PHYS_STATIC, (int)out.type);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, out.mass);
}

