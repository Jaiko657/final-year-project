#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "modules/core/logger.h"

#define TEST_LOG_SINK_MAX_RECORDS 128
#define TEST_LOG_SINK_ANY_LEVEL ((log_level_t)-1)

typedef struct {
    log_level_t lvl;
    char category[32];
    char message[512];
} test_log_sink_record_t;

void test_log_sink_install(void);
void test_log_sink_restore(void);
void test_log_sink_reset(void);
size_t test_log_sink_count(void);
const test_log_sink_record_t* test_log_sink_record(size_t index);
bool test_log_sink_contains(log_level_t lvl, const char* category, const char* message_substring);
