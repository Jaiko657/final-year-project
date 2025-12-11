#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "raylib.h"
#include "asset.h"

typedef struct {
    int tilewidth;
    int tileheight;
    int tilecount;
    int columns;
    int first_gid;
    int image_width;
    int image_height;
    char *image_path; // heap-allocated
    uint16_t *colliders; // tilecount entries, 4x4 subtile masks
} tiled_tileset_t;

typedef struct {
    char *name;       // heap-allocated
    int width;
    int height;
    uint32_t *gids;   // width*height entries
    bool collision;   // property flag
} tiled_layer_t;

typedef struct {
    int width;
    int height;
    int tilewidth;
    int tileheight;
    tiled_tileset_t tileset;
    size_t layer_count;
    tiled_layer_t *layers; // layer_count entries
} tiled_map_t;

bool tiled_load_map(const char *tmx_path, tiled_map_t *out_map);
void tiled_free_map(tiled_map_t *map);

typedef struct {
    tex_handle_t tileset;
} tiled_renderer_t;

bool tiled_renderer_init(tiled_renderer_t *r, const tiled_map_t *map);
void tiled_renderer_shutdown(tiled_renderer_t *r);
void tiled_renderer_draw(const tiled_map_t *map, const tiled_renderer_t *r, const Rectangle *view);
