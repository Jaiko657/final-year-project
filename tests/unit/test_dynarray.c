#include "unity.h"

#include "dynarray.h"

typedef struct {
    int v;
} item_t;

void test_dynarray_append_and_clear(void)
{
    DA(item_t) items = {0};

    DA_APPEND(&items, (item_t){ .v = 1 });
    DA_APPEND(&items, (item_t){ .v = 2 });
    DA_APPEND(&items, (item_t){ .v = 3 });

    TEST_ASSERT_EQUAL_UINT32(3, (uint32_t)items.size);
    TEST_ASSERT_TRUE(items.capacity >= items.size);
    TEST_ASSERT_EQUAL_INT(1, items.data[0].v);
    TEST_ASSERT_EQUAL_INT(3, items.data[2].v);

    DA_CLEAR(&items);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)items.size);
    TEST_ASSERT_NOT_NULL(items.data); // capacity retained

    DA_FREE(&items);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)items.size);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)items.capacity);
    TEST_ASSERT_NULL(items.data);
}
