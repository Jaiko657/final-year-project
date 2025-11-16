#include "../includes/logger.h"
#include <stdio.h>
#include <stdarg.h>

static log_sink_fn g_sink = NULL;
static log_level_t g_min  = LOG_LVL_TRACE;

static void default_sink(log_level_t lvl, const log_cat_t* cat, const char* fmt, va_list ap){
    static const char* N[]={"TRACE","DEBUG","INFO","WARN","ERROR","FATAL"};
    fprintf(stderr, "[%s]%s%s%s", N[lvl],
            cat && cat->name ? "[" : "",
            cat && cat->name ? cat->name : "",
            cat && cat->name ? "] " : " ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}

void log_set_sink(log_sink_fn sink){ g_sink = sink; }
void log_set_min_level(log_level_t lvl){ g_min = lvl; }
bool log_would_log(log_level_t lvl){ return lvl >= g_min; }

void log_msg(log_level_t lvl, const log_cat_t* cat, const char* fmt, ...){
    va_list ap; va_start(ap, fmt);
    (g_sink ? g_sink : default_sink)(lvl, cat, fmt, ap);
    va_end(ap);
}
