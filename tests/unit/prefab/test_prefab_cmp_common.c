#include "unity.h"

#include <stdbool.h>
#include <stdint.h>

#include "modules/prefab/prefab_cmp.h"

void test_prefab_parse_int_parses_decimal_and_hex(void)
{
    int v = 0;
    TEST_ASSERT_TRUE(prefab_parse_int("123", &v));
    TEST_ASSERT_EQUAL_INT(123, v);

    v = 0;
    TEST_ASSERT_TRUE(prefab_parse_int("0x10", &v));
    TEST_ASSERT_EQUAL_INT(16, v);
}

void test_prefab_parse_int_rejects_invalid(void)
{
    int v = 7;
    TEST_ASSERT_FALSE(prefab_parse_int(NULL, &v));
    TEST_ASSERT_FALSE(prefab_parse_int("nope", &v));
}

void test_prefab_parse_mask_parses_names_and_sets_out_ok(void)
{
    bool ok = false;
    uint32_t mask = prefab_parse_mask("ITEM|COL", &ok);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE((mask & CMP_ITEM) != 0);
    TEST_ASSERT_TRUE((mask & CMP_COL) != 0);
}

void test_prefab_parse_mask_accepts_commas_spaces_and_numeric_fallback(void)
{
    bool ok = false;
    uint32_t mask = prefab_parse_mask(" ITEM , COL ", &ok);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE((mask & CMP_ITEM) != 0);
    TEST_ASSERT_TRUE((mask & CMP_COL) != 0);

    ok = false;
    mask = prefab_parse_mask("0x3", &ok);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(0x3u, mask);
}

void test_prefab_parse_mask_invalid_sets_out_ok_false(void)
{
    bool ok = true;
    uint32_t mask = prefab_parse_mask("NOT_A_COMPONENT", &ok);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_UINT32(0u, mask);
}

