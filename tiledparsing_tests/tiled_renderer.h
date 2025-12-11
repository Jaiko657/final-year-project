#pragma once
#include "tiled_parser.h"
#include "raylib.h"

typedef struct {
    size_t texture_count;
    Texture2D *tileset_textures; // matches map->tilesets ordering
} tiled_renderer_t;

bool tiled_renderer_init(tiled_renderer_t *r, const tiled_map_t *map);
void tiled_renderer_shutdown(tiled_renderer_t *r);
void tiled_renderer_draw(const tiled_map_t *map, const tiled_renderer_t *r);
