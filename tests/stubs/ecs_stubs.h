#pragma once

#include <stdbool.h>

#include "ecs.h"

void ecs_stub_reset(void);
void ecs_stub_set_target(ecs_entity_t target);
void ecs_stub_set_position(bool has_position, v2f position);

