#include "modules/core/logger_raylib_adapter.h"
#include "modules/core/logger.h"

#include <stdio.h>

static void stdout_sink(log_level_t lvl, const log_cat_t* cat, const char* fmt, va_list ap)
{
    static const char* N[] = { "TRACE","DEBUG","INFO","WARN","ERROR","FATAL" };
    fprintf(stdout, "[%s]%s%s%s", N[lvl],
            cat && cat->name ? "[" : "",
            cat && cat->name ? cat->name : "",
            cat && cat->name ? "] " : " ");
    vfprintf(stdout, fmt, ap);
    fputc('\n', stdout);
    fflush(stdout);
}

void logger_use_raylib(void)
{
    // In headless mode, keep the same callsite but redirect to stdout.
    log_set_sink(stdout_sink);
}

