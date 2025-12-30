#include "raylib.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int g_last_level = 0;
static char g_last_msg[512];

void raylib_log_stub_reset(void)
{
    g_last_level = 0;
    g_last_msg[0] = '\0';
}

int raylib_log_stub_last_level(void)
{
    return g_last_level;
}

const char *raylib_log_stub_last_message(void)
{
    return g_last_msg;
}

void TraceLog(int logLevel, const char *text, ...)
{
    g_last_level = logLevel;
    va_list ap;
    va_start(ap, text);
    vsnprintf(g_last_msg, sizeof(g_last_msg), text, ap);
    va_end(ap);
}
