#include "test_log_sink.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static test_log_sink_record_t g_records[TEST_LOG_SINK_MAX_RECORDS];
static size_t g_record_count;

static void test_log_sink_impl(log_level_t lvl, const log_cat_t* cat, const char* fmt, va_list ap)
{
    if (g_record_count >= TEST_LOG_SINK_MAX_RECORDS) return;
    test_log_sink_record_t* rec = &g_records[g_record_count++];
    rec->lvl = lvl;
    if (cat && cat->name) {
        snprintf(rec->category, sizeof(rec->category), "%s", cat->name);
    } else {
        rec->category[0] = '\0';
    }
    vsnprintf(rec->message, sizeof(rec->message), fmt, ap);
}

void test_log_sink_install(void)
{
    log_set_sink(test_log_sink_impl);
}

void test_log_sink_restore(void)
{
    log_set_sink(NULL);
}

void test_log_sink_reset(void)
{
    g_record_count = 0;
}

size_t test_log_sink_count(void)
{
    return g_record_count;
}

const test_log_sink_record_t* test_log_sink_record(size_t index)
{
    return index < g_record_count ? &g_records[index] : NULL;
}

bool test_log_sink_contains(log_level_t lvl, const char* category, const char* message_substring)
{
    for (size_t i = 0; i < g_record_count; ++i) {
        const test_log_sink_record_t* rec = &g_records[i];
        if (lvl != TEST_LOG_SINK_ANY_LEVEL && rec->lvl != lvl) continue;
        if (category && strcmp(rec->category, category) != 0) continue;
        if (message_substring && !strstr(rec->message, message_substring)) continue;
        return true;
    }
    return false;
}
