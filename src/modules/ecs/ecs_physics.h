#pragma once
#include "modules/ecs/ecs_physics_types.h"

// Internal helpers (component indices)
void ecs_phys_body_create_for_entity(int idx);
void ecs_phys_body_destroy_for_entity(int idx);
void ecs_phys_destroy_all(void);
