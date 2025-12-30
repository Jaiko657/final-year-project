#include "unity.h"

#include <string.h>

#include "modules/core/toast.h"

void test_toast_init_clears_state(void)
{
    ui_toast(1.0f, "hello");
    TEST_ASSERT_TRUE(ecs_toast_is_active());

    ui_toast_init();
    TEST_ASSERT_FALSE(ecs_toast_is_active());
    TEST_ASSERT_EQUAL_STRING("", ecs_toast_get_text());
}

void test_toast_sets_text_and_expires(void)
{
    ui_toast_init();
    ui_toast(0.5f, "coins=%d", 3);

    TEST_ASSERT_TRUE(ecs_toast_is_active());
    TEST_ASSERT_EQUAL_STRING("coins=3", ecs_toast_get_text());

    ui_toast_update(0.25f);
    TEST_ASSERT_TRUE(ecs_toast_is_active());

    ui_toast_update(0.25f);
    TEST_ASSERT_FALSE(ecs_toast_is_active());
}

void test_toast_update_clamps_to_zero(void)
{
    ui_toast_init();
    ui_toast(0.1f, "x");
    ui_toast_update(10.0f);
    TEST_ASSERT_FALSE(ecs_toast_is_active());
}

void test_toast_text_is_truncated_safely(void)
{
    ui_toast_init();

    char big[256];
    memset(big, 'a', sizeof(big));
    big[sizeof(big) - 1] = '\0';

    ui_toast(1.0f, "%s", big);

    const char *t = ecs_toast_get_text();
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_TRUE(strlen(t) <= 127);
}
