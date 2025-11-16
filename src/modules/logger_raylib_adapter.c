#include "../includes/logger.h"
#include "../includes/logger_raylib_adapter.h"
#include "raylib.h"
#include <stdio.h>
#include <stddef.h>

static void raylib_sink(log_level_t lvl, const log_cat_t* cat, const char* fmt, va_list ap){
    int rl = LOG_INFO; // Raylib's levels
    switch(lvl){
        case LOG_LVL_TRACE: rl = LOG_TRACE;   break;
        case LOG_LVL_DEBUG: rl = LOG_DEBUG;   break;
        case LOG_LVL_INFO:  rl = LOG_INFO;    break;
        case LOG_LVL_WARN:  rl = LOG_WARNING; break;
        case LOG_LVL_ERROR: rl = LOG_ERROR;   break;
        case LOG_LVL_FATAL: rl = LOG_FATAL;   break;
    }

    char msg[1024];
    int n = 0;
    if (cat && cat->name) n = snprintf(msg, sizeof(msg), "[%s] ", cat->name);
    vsnprintf(msg + n, sizeof(msg) - (size_t)n, fmt, ap);
    TraceLog(rl, "%s", msg);
}

void logger_use_raylib(void){
    log_set_sink(raylib_sink);
}
