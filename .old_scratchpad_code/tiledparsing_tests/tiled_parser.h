#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

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
    tiled_animation_t *anims; // tilecount entries
} tiled_tileset_t;

typedef struct {
    char *name;       // heap-allocated
    int width;
    int height;
    uint32_t *gids;   // width*height entries
} tiled_layer_t;

typedef struct {
    int width;
    int height;
    int tilewidth;
    int tileheight;
    size_t tileset_count;
    tiled_tileset_t *tilesets; // tileset_count entries
    size_t layer_count;
    tiled_layer_t *layers; // layer_count entries
} tiled_map_t;

bool tiled_load_map(const char *tmx_path, tiled_map_t *out_map);
void tiled_free_map(tiled_map_t *map);
