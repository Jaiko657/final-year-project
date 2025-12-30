#include "logger_adapter_stubs.h"

#include <stdarg.h>
#include <stddef.h>

static log_sink_fn g_sink = NULL;

void logger_adapter_stub_reset(void)
{
    g_sink = NULL;
}

log_sink_fn logger_adapter_stub_sink(void)
{
    return g_sink;
}

void log_set_sink(log_sink_fn sink)
{
    g_sink = sink;
}

void logger_adapter_stub_invoke(log_level_t lvl, const log_cat_t* cat, const char* fmt, ...)
{
    if (!g_sink) return;
    va_list ap;
    va_start(ap, fmt);
    g_sink(lvl, cat, fmt, ap);
    va_end(ap);
}
