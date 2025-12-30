#pragma once

#include "modules/core/logger.h"

void logger_adapter_stub_reset(void);
log_sink_fn logger_adapter_stub_sink(void);
void logger_adapter_stub_invoke(log_level_t lvl, const log_cat_t* cat, const char* fmt, ...);
