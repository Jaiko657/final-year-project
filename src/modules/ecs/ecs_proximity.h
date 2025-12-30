#pragma once
#include <stdbool.h>
#include "modules/ecs/ecs.h"

// View of a proximity pair (trigger-owner, target)
typedef struct {
    ecs_entity_t trigger_owner;   // the entity that owns CMP_TRIGGER
    ecs_entity_t matched_entity;   // the entity that matched filter & is within pad
} ecs_prox_view_t;

typedef struct { int i; } ecs_prox_iter_t;

ecs_prox_iter_t ecs_prox_stay_begin(void);
bool            ecs_prox_stay_next(ecs_prox_iter_t* it, ecs_prox_view_t* out);

ecs_prox_iter_t ecs_prox_enter_begin(void);
bool            ecs_prox_enter_next(ecs_prox_iter_t* it, ecs_prox_view_t* out);

ecs_prox_iter_t ecs_prox_exit_begin(void);
bool            ecs_prox_exit_next(ecs_prox_iter_t* it, ecs_prox_view_t* out);
