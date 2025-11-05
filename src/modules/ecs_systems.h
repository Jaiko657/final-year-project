#pragma once
#include <stdint.h>
#include <stddef.h>
#include "input.h"

// Phases
typedef enum {
    PHASE_INPUT = 0,
    PHASE_SIM_PRE,
    PHASE_PHYSICS,
    PHASE_SIM_POST,
    PHASE_DEBUG,
    PHASE_RENDER_FEED,
    PHASE_COUNT
    // I iterate until PHASE_COUNT, dont put any enum after it
} ecs_phase_t;

// System signature
typedef void (*ecs_system_fn)(float dt, const input_t* in);

// API
void ecs_systems_init(void);
void ecs_register_system(ecs_phase_t phase, int order, ecs_system_fn fn, const char* name);
void ecs_run_phase(ecs_phase_t phase, float dt, const input_t* in);

// Optional: quick query (useful for future perf tooling)
typedef struct {
    const char* name;
    int order;
    ecs_system_fn fn;
} ecs_system_info_t;

size_t ecs_get_phase_systems(ecs_phase_t phase, const ecs_system_info_t** out_list);
