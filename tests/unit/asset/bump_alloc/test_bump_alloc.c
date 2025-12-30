#include "unity.h"

#include <stdint.h>

#include "modules/asset/bump_alloc.h"

void test_bump_init_sets_fields(void)
{
    bump_alloc_t b = {0};
    TEST_ASSERT_TRUE(bump_init(&b, 64));
    TEST_ASSERT_NOT_NULL(b.data);
    TEST_ASSERT_EQUAL_UINT32(64, (uint32_t)b.capacity);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)b.offset);
    bump_free(&b);
}

void test_bump_alloc_alignment_and_offset(void)
{
    bump_alloc_t b = {0};
    TEST_ASSERT_TRUE(bump_init(&b, 64));

    void *p1 = bump_alloc_aligned(&b, 1, 8);
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)((uintptr_t)p1 % 8u));

    size_t offset_after_1 = b.offset;
    TEST_ASSERT_TRUE(offset_after_1 >= 1);

    void *p2 = bump_alloc_aligned(&b, 8, 16);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)((uintptr_t)p2 % 16u));
    TEST_ASSERT_TRUE(b.offset > offset_after_1);

    bump_free(&b);
}

void test_bump_alloc_overflow_fails_without_advancing_offset(void)
{
    bump_alloc_t b = {0};
    TEST_ASSERT_TRUE(bump_init(&b, 32));

    void *p1 = bump_alloc_aligned(&b, 24, 8);
    TEST_ASSERT_NOT_NULL(p1);
    size_t before = b.offset;

    void *p2 = bump_alloc_aligned(&b, 16, 8);
    TEST_ASSERT_NULL(p2);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)before, (uint32_t)b.offset);

    bump_free(&b);
}

void test_bump_reset_reuses_buffer(void)
{
    bump_alloc_t b = {0};
    TEST_ASSERT_TRUE(bump_init(&b, 64));

    void *p1 = bump_alloc_aligned(&b, 8, 8);
    TEST_ASSERT_NOT_NULL(p1);

    bump_reset(&b);

    void *p2 = bump_alloc_aligned(&b, 8, 8);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_EQUAL_PTR(p1, p2);

    bump_free(&b);
}
