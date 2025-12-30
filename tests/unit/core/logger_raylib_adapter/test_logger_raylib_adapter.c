#include "unity.h"

#include "modules/core/logger_raylib_adapter.h"
#include "logger_adapter_stubs.h"
#include "raylib.h"

void setUp(void)
{
    logger_adapter_stub_reset();
    raylib_log_stub_reset();
}

void tearDown(void)
{
}

void test_logger_use_raylib_installs_sink(void)
{
    TEST_ASSERT_NULL(logger_adapter_stub_sink());
    logger_use_raylib();
    TEST_ASSERT_NOT_NULL(logger_adapter_stub_sink());
}

void test_raylib_sink_maps_levels_and_formats_category(void)
{
    logger_use_raylib();

    log_cat_t cat = { "ECS" };
    logger_adapter_stub_invoke(LOG_LVL_WARN, &cat, "x=%d", 3);

    TEST_ASSERT_EQUAL_INT(LOG_WARNING, raylib_log_stub_last_level());
    TEST_ASSERT_EQUAL_STRING("[ECS] x=3", raylib_log_stub_last_message());
}

void test_raylib_sink_handles_null_category(void)
{
    logger_use_raylib();

    logger_adapter_stub_invoke(LOG_LVL_ERROR, NULL, "oops");

    TEST_ASSERT_EQUAL_INT(LOG_ERROR, raylib_log_stub_last_level());
    TEST_ASSERT_EQUAL_STRING("oops", raylib_log_stub_last_message());
}

void test_raylib_sink_maps_trace_level(void)
{
    logger_use_raylib();

    log_cat_t cat = { "SYS" };
    logger_adapter_stub_invoke(LOG_LVL_TRACE, &cat, "trace");

    TEST_ASSERT_EQUAL_INT(LOG_TRACE, raylib_log_stub_last_level());
    TEST_ASSERT_EQUAL_STRING("[SYS] trace", raylib_log_stub_last_message());
}
