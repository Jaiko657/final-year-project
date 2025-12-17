#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "engine_types.h"
#include "tiled.h"

typedef enum {
    WORLD_TILE_VOID = 0,   // out of bounds / unknown
    WORLD_TILE_WALKABLE,
    WORLD_TILE_SOLID
} world_tile_t;

// Tile size is fixed for now (pixels per tile)
int  world_tile_size(void);
int  world_subtile_size(void);

// Lifecycle
bool world_load_from_tmx(const char* tmx_path, const char* collision_layer_name);
void world_shutdown(void);

// Queries
void world_size_tiles(int* out_w, int* out_h);
void world_size_px(int* out_w, int* out_h);
world_tile_t world_tile_at(int tx, int ty);
uint16_t world_subtile_mask_at(int tx, int ty);
bool world_tile_is_dynamic(int tx, int ty);
bool world_is_walkable_px(float x, float y);
bool world_is_walkable_subtile(int sx, int sy);
bool world_is_walkable_rect_px(float cx, float cy, float hx, float hy);
bool world_has_line_of_sight(float x0, float y0, float x1, float y1, float max_range, float hx, float hy);
v2f  world_get_spawn_px(void);

// Optional runtime sync: update collision masks from a tiled map (e.g., for animated tiles)
void world_sync_tiled_colliders(const tiled_map_t *map);
