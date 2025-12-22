#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "tiled.h"

// Pure helper: decode TMX raw GID (with flip flags) into a 4x4 subtile collision mask.
// Also returns whether this tile is marked as "dynamic/no-merge" in the tileset.
bool world_collision_decode_raw_gid(const tiled_map_t* map, uint32_t raw_gid, uint16_t* out_mask, bool* out_dynamic);

// Internal helpers used by the world map owner when tiles change.
bool world_collision_build_from_map(tiled_map_t* map, const char* collision_layer_name);
void world_collision_shutdown(void);
void world_collision_refresh_tile(const tiled_map_t* map, int tx, int ty);

