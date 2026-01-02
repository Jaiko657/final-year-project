#include "modules/systems/systems.h"
#include "modules/core/logger.h"
#include <string.h>

#ifndef ECS_MAX_SYSTEMS_PER_PHASE
#define ECS_MAX_SYSTEMS_PER_PHASE 64
#endif

typedef struct {
    const char* name;
    int order;
    systems_fn fn;
} sys_rec_t;

static sys_rec_t g_systems[PHASE_COUNT][ECS_MAX_SYSTEMS_PER_PHASE];
static size_t    g_counts[PHASE_COUNT];

static void sort_phase(systems_phase_t phase)
{
    size_t n = g_counts[phase];
    for (size_t i = 1; i < n; ++i) {
        sys_rec_t key = g_systems[phase][i];
        size_t j = i;
        while (j > 0 && g_systems[phase][j-1].order > key.order) {
            g_systems[phase][j] = g_systems[phase][j-1];
            --j;
        }
        g_systems[phase][j] = key;
    }
}

void systems_init(void)
{
    for (int p = 0; p < (int)PHASE_COUNT; ++p) {
        g_counts[p] = 0;
    }
}

void systems_register(systems_phase_t phase, int order, systems_fn fn, const char* name)
{
    if ((int)phase < 0 || phase >= PHASE_COUNT) {
        LOGC(LOGCAT("SYS"), LOG_LVL_WARN, "systems: invalid phase %d for %s", phase, name ? name : "(unnamed)");
        return;
    }
    size_t* cnt = &g_counts[phase];
    if (*cnt >= ECS_MAX_SYSTEMS_PER_PHASE) {
        LOGC(LOGCAT("SYS"), LOG_LVL_ERROR, "systems: phase %d full; can't register %s", phase, name ? name : "(unnamed)");
        return;
    }

    g_systems[phase][*cnt] = (sys_rec_t){ name, order, fn };
    (*cnt)++;
    sort_phase(phase);
}

void systems_run_phase(systems_phase_t phase, float dt, const input_t* in)
{
    if ((int)phase < 0 || phase >= PHASE_COUNT) return;
    size_t n = g_counts[phase];
    for (size_t i = 0; i < n; ++i) {
        systems_fn fn = g_systems[phase][i].fn;
        if (fn) fn(dt, in);
    }
}

size_t systems_get_phase_systems(systems_phase_t phase, const systems_info_t** out_list)
{
    if ((int)phase < 0 || phase >= PHASE_COUNT) {
        if (out_list) *out_list = NULL;
        return 0;
    }
    if (out_list) *out_list = (const systems_info_t*)g_systems[phase];
    return g_counts[phase];
}

void systems_tick(float dt, const input_t* in)
{
    systems_run_phase(PHASE_INPUT,    dt, in);
    systems_run_phase(PHASE_SIM_PRE,  dt, in);
    systems_run_phase(PHASE_PHYSICS,  dt, in);
    systems_run_phase(PHASE_SIM_POST, dt, in);
    systems_run_phase(PHASE_DEBUG,    dt, in);
}

void systems_present(float frame_dt)
{
    systems_run_phase(PHASE_PRESENT, frame_dt, NULL);
    systems_run_phase(PHASE_RENDER, frame_dt, NULL);
}
