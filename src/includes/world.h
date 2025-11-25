#pragma once
#include <stdbool.h>
#include "engine_types.h"

typedef enum {
    WORLD_TILE_VOID = 0,   // out of bounds / unknown
    WORLD_TILE_WALKABLE,
    WORLD_TILE_SOLID
} world_tile_t;

// Tile size is fixed for now (pixels per tile)
int  world_tile_size(void);

// Lifecycle
bool world_load(const char* path);
void world_shutdown(void);

// Queries
void world_size_tiles(int* out_w, int* out_h);
void world_size_px(int* out_w, int* out_h);
world_tile_t world_tile_at(int tx, int ty);
bool world_is_walkable_px(float x, float y);
v2f  world_get_spawn_px(void);
