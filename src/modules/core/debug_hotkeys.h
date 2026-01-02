#pragma once

#include "modules/core/build_config.h"

#if DEBUG_BUILD
void debug_post_frame(void);
#else
static inline void debug_post_frame(void) { }
#endif
