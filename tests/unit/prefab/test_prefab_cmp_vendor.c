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

void test_prefab_cmp_vendor_build_parses_sells_and_price(void)
{
    prefab_kv_t props[] = {
        { .name = "sells", .value = "coin" },
        { .name = "price", .value = "5" },
    };
    prefab_component_t comp = make_comp("VENDOR", props, 2);

    prefab_cmp_vendor_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_vendor_build(&comp, NULL, &out));
    TEST_ASSERT_EQUAL_INT(ITEM_COIN, (int)out.sells);
    TEST_ASSERT_EQUAL_INT(5, out.price);
}

