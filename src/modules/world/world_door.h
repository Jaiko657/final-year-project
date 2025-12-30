#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "modules/common/resource_handles.h"
#include "modules/world/door_tiles.h"
#define WORLD_DOOR_INVALID_HANDLE 0

world_door_handle_t world_door_register(const door_tile_xy_t* tile_xy, size_t tile_count);
void world_door_unregister(world_door_handle_t handle);
int world_door_primary_animation_duration(world_door_handle_t handle);
void world_door_apply_state(world_door_handle_t handle, float t_ms, bool play_forward);
void world_door_shutdown(void);
