#include "../includes/ecs_systems.h"
#include <string.h>

#ifndef ECS_MAX_SYSTEMS_PER_PHASE
#define ECS_MAX_SYSTEMS_PER_PHASE 64
#endif

typedef struct {
    const char* name;
    int order;
    ecs_system_fn fn;
} sys_rec_t;

static sys_rec_t g_systems[PHASE_COUNT][ECS_MAX_SYSTEMS_PER_PHASE];
static size_t    g_counts[PHASE_COUNT];

static void sort_phase(ecs_phase_t p)
{
    size_t n = g_counts[p];
    // insertion sort, grand for small array size
    for (size_t i = 1; i < n; ++i) {
        sys_rec_t key = g_systems[p][i];
        size_t j = i;
        while (j > 0 && g_systems[p][j-1].order > key.order) {
            g_systems[p][j] = g_systems[p][j-1];
            --j;
        }
        g_systems[p][j] = key;
    }
}

void ecs_systems_init(void)
{
    for (int p = 0; p < (int)PHASE_COUNT; ++p) {
        g_counts[p] = 0;
    }
}

void ecs_register_system(ecs_phase_t phase, int order, ecs_system_fn fn, const char* name)
{
    if ((int)phase < 0 || phase >= PHASE_COUNT) return;
    size_t* cnt = &g_counts[phase];
    if (*cnt >= ECS_MAX_SYSTEMS_PER_PHASE) return;

    g_systems[phase][*cnt] = (sys_rec_t){ name, order, fn };
    (*cnt)++;
    sort_phase(phase);
}

void ecs_run_phase(ecs_phase_t phase, float dt, const input_t* in)
{
    if ((int)phase < 0 || phase >= PHASE_COUNT) return;
    size_t n = g_counts[phase];
    for (size_t i = 0; i < n; ++i) {
        ecs_system_fn fn = g_systems[phase][i].fn;
        if (fn) fn(dt, in);
    }
}

size_t ecs_get_phase_systems(ecs_phase_t phase, const ecs_system_info_t** out_list)
{
    if ((int)phase < 0 || phase >= PHASE_COUNT) { if (out_list) *out_list = NULL; return 0; }
    if (out_list) *out_list = (const ecs_system_info_t*)g_systems[phase];
    return g_counts[phase];
}
