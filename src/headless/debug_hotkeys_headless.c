#include "modules/core/build_config.h"

#if DEBUG_BUILD

#include "modules/core/input.h"
#include "modules/systems/systems_registration.h"
#include "modules/core/debug_hotkeys.h"

void sys_debug_binds(const input_t* in)
{
    (void)in;
}

void debug_post_frame(void)
{
}

SYSTEMS_ADAPT_INPUT(sys_debug_binds_adapt, sys_debug_binds)

#endif
