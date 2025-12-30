#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "modules/tiled/tiled.h"

// Derived collision grid helpers used by the world map owner when tiles change.
bool world_collision_decode_raw_gid(const world_map_t* map, uint32_t raw_gid, uint16_t* out_mask, bool* out_dynamic);
bool world_collision_build_from_map(world_map_t* map, const char* collision_layer_name);
void world_collision_shutdown(void);
void world_collision_refresh_tile(const world_map_t* map, int tx, int ty);
