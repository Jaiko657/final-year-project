#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "raylib.h"
#include "asset.h"

typedef struct {
    int tile_id;
    int duration_ms;
} tiled_anim_frame_t;

typedef struct {
    tiled_anim_frame_t *frames; // frame_count entries, heap-allocated
    size_t frame_count;
    int total_duration_ms;
} tiled_animation_t;

typedef struct {
    int tilewidth;
    int tileheight;
    int tilecount;
    int columns;
    int first_gid;
    int image_width;
    int image_height;
    char *image_path;      // heap-allocated
    uint16_t *colliders;   // tilecount entries, 4x4 subtile masks
    tiled_animation_t *anims; // tilecount entries, optional
} tiled_tileset_t;

typedef struct {
    char *name;       // heap-allocated
    int width;
    int height;
    uint32_t *gids;   // width*height entries
    bool collision;   // property flag
} tiled_layer_t;

typedef struct {
    int id;
    int gid;
    float x, y, w, h; // pixels
    char *animationtype;   // optional
    int   proximity_radius;
    int   proximity_off_x;
    int   proximity_off_y;
    int   door_tile_count;
    int   door_tiles[4][2]; // tile coordinates (x,y) this object controls, up to 4
} tiled_object_t;

typedef struct {
    int width;
    int height;
    int tilewidth;
    int tileheight;
    size_t tileset_count;
    tiled_tileset_t *tilesets; // tileset_count entries
    size_t layer_count;
    tiled_layer_t *layers; // layer_count entries
    size_t object_count;
    tiled_object_t *objects; // object_count entries
} tiled_map_t;

bool tiled_load_map(const char *tmx_path, tiled_map_t *out_map);
void tiled_free_map(tiled_map_t *map);

typedef struct {
    size_t texture_count;
    tex_handle_t *tilesets; // matches map->tilesets ordering
} tiled_renderer_t;

bool tiled_renderer_init(tiled_renderer_t *r, const tiled_map_t *map);
void tiled_renderer_shutdown(tiled_renderer_t *r);
void tiled_renderer_draw(const tiled_map_t *map, const tiled_renderer_t *r, const Rectangle *view);
