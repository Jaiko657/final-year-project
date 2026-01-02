#include "modules/ecs/ecs_internal.h"
#include "modules/core/effects.h"
#include "modules/core/input.h"
#include "modules/systems/systems_registration.h"

static void sys_effects_tick_begin_impl(void)
{
    fx_lines_clear();
    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & CMP_SPR) == 0) continue;
        cmp_spr[i].fx.highlighted = false;
    }
}

SYSTEMS_ADAPT_VOID(sys_effects_tick_begin_adapt, sys_effects_tick_begin_impl)
