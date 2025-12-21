#include "unity.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "logger.h"

typedef struct {
    int calls;
    log_level_t lvl;
    char cat[32];
    char msg[256];
} sink_state_t;

static sink_state_t g_sink;

static void test_sink(log_level_t lvl, const log_cat_t* cat, const char* fmt, va_list ap)
{
    g_sink.calls++;
    g_sink.lvl = lvl;
    snprintf(g_sink.cat, sizeof(g_sink.cat), "%s", (cat && cat->name) ? cat->name : "");
    vsnprintf(g_sink.msg, sizeof(g_sink.msg), fmt, ap);
}

void test_logger_min_level_gate(void)
{
    log_set_sink(test_sink);
    log_set_min_level(LOG_LVL_WARN);

    TEST_ASSERT_FALSE(log_would_log(LOG_LVL_INFO));
    TEST_ASSERT_TRUE(log_would_log(LOG_LVL_WARN));
    TEST_ASSERT_TRUE(log_would_log(LOG_LVL_FATAL));
}

void test_logger_message_goes_to_sink(void)
{
    memset(&g_sink, 0, sizeof(g_sink));
    log_set_sink(test_sink);
    log_set_min_level(LOG_LVL_TRACE);

    log_msg(LOG_LVL_INFO, &LOGCAT_ECS, "x=%d y=%s", 3, "hi");

    TEST_ASSERT_EQUAL_INT(1, g_sink.calls);
    TEST_ASSERT_EQUAL_INT(LOG_LVL_INFO, g_sink.lvl);
    TEST_ASSERT_EQUAL_STRING("ECS", g_sink.cat);
    TEST_ASSERT_EQUAL_STRING("x=3 y=hi", g_sink.msg);
}

void test_logger_null_category_is_ok(void)
{
    memset(&g_sink, 0, sizeof(g_sink));
    log_set_sink(test_sink);
    log_set_min_level(LOG_LVL_TRACE);

    log_msg(LOG_LVL_WARN, NULL, "no-cat");

    TEST_ASSERT_EQUAL_INT(1, g_sink.calls);
    TEST_ASSERT_EQUAL_INT(LOG_LVL_WARN, g_sink.lvl);
    TEST_ASSERT_EQUAL_STRING("", g_sink.cat);
    TEST_ASSERT_EQUAL_STRING("no-cat", g_sink.msg);
}

void test_logger_default_sink_path_does_not_crash(void)
{
    // This mostly exists to cover the default sink code path.
    log_set_sink(NULL);
    log_set_min_level(LOG_LVL_TRACE);

    log_msg(LOG_LVL_INFO, &LOGCAT_MAIN, "default sink %d", 1);

    log_cat_t anon = {0};
    log_msg(LOG_LVL_INFO, &anon, "anon cat");
}
