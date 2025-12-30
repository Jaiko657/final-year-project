#pragma once

#define LOG_TRACE 0
#define LOG_DEBUG 1
#define LOG_INFO 2
#define LOG_WARNING 3
#define LOG_ERROR 4
#define LOG_FATAL 5

void TraceLog(int logLevel, const char *text, ...);

void raylib_log_stub_reset(void);
int raylib_log_stub_last_level(void);
const char *raylib_log_stub_last_message(void);
