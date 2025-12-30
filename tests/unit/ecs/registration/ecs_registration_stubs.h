#pragma once

#include "modules/systems/systems.h"

typedef struct {
    systems_phase_t phase;
    int order;
    const char* name;
    systems_fn fn;
    int seq;
} systems_registration_call_t;

extern systems_registration_call_t g_systems_registration_calls[32];
extern int g_systems_registration_call_count;
extern int g_systems_init_seq;
extern int g_ecs_render_hooks_seq;
extern int g_ecs_physics_hooks_seq;
extern int g_ecs_game_hooks_seq;
extern int g_ecs_door_hooks_seq;

void ecs_registration_stubs_reset(void);
