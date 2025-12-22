#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "tiled.h"

// Lifecycle (owns TMX runtime map)
bool world_load_from_tmx(const char* tmx_path, const char* collision_layer_name);
void world_shutdown(void);

// Runtime TMX (world-owned; read-only access)
const tiled_map_t* world_get_tiled_map(void);
uint32_t world_map_generation(void);

// Runtime tile edits (queued; applied later via `world_apply_tile_edits()`).
bool world_set_tile_gid(int layer_idx, int tx, int ty, uint32_t raw_gid);
void world_apply_tile_edits(void);

#if defined(UNIT_TEST) && UNIT_TEST
// Test helper: adopt an already-constructed map (takes ownership of heap allocations inside it).
bool world_test_set_map(tiled_map_t* map, const char* collision_layer_name);
#endif

