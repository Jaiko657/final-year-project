#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "modules/tiled/tiled_types.h"

typedef struct {
    int width_tiles;
    int height_tiles;
    int tile_width;
    int tile_height;
    int layer_count;
    int object_count;
} world_map_info_t;

typedef struct {
    int width;
    int height;
    int tilewidth;
    int tileheight;
    size_t tileset_count;
    tiled_tileset_t *tilesets;
    size_t layer_count;
    tiled_layer_t *layers;
    size_t object_count;
    tiled_object_t *objects;
} world_map_t;

// Lifecycle (owns TMX runtime map)
bool world_load_from_tmx(const char* tmx_path, const char* collision_layer_name);
void world_shutdown(void);

bool world_has_map(void);
bool world_get_map_info(world_map_info_t* out);
uint32_t world_map_generation(void);

// Runtime tile edits (queued; applied later via `world_apply_tile_edits()`).
bool world_set_tile_gid(int layer_idx, int tx, int ty, uint32_t raw_gid);
void world_apply_tile_edits(void);
