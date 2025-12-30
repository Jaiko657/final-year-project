#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
// TMX global tile IDs (GIDs) encode flip flags in the top bits.
// Ref: https://doc.mapeditor.org/en/stable/reference/tmx-map-format/
#define TILED_FLIPPED_HORIZONTALLY_FLAG 0x80000000u
#define TILED_FLIPPED_VERTICALLY_FLAG   0x40000000u
#define TILED_FLIPPED_DIAGONALLY_FLAG   0x20000000u
#define TILED_GID_MASK                  0x1FFFFFFFu

static inline uint32_t tiled_gid_strip_flags(uint32_t raw_gid,
                                             bool* out_flip_h,
                                             bool* out_flip_v,
                                             bool* out_flip_d)
{
    if (out_flip_h) *out_flip_h = (raw_gid & TILED_FLIPPED_HORIZONTALLY_FLAG) != 0;
    if (out_flip_v) *out_flip_v = (raw_gid & TILED_FLIPPED_VERTICALLY_FLAG) != 0;
    if (out_flip_d) *out_flip_d = (raw_gid & TILED_FLIPPED_DIAGONALLY_FLAG) != 0;
    return raw_gid & TILED_GID_MASK;
}

typedef struct {
    int tile_id;
    int duration_ms;
} tiled_anim_frame_t;

typedef struct {
    tiled_anim_frame_t *frames;
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
    char *image_path;
    uint16_t *colliders;
    bool *no_merge_collider;
    tiled_animation_t *anims;
    bool *render_painters;
    int  *painter_offset;
} tiled_tileset_t;

typedef struct {
    char *name;
    int width;
    int height;
    uint32_t *gids;
    bool collision;
    int z_order;
} tiled_layer_t;

typedef struct {
    char *name;
    char *type;
    char *value;
} tiled_property_t;

typedef struct {
    int id;
    uint32_t gid;
    char *layer_name;
    int   layer_z;
    char *name;
    float x, y, w, h;
    char *animationtype;
    int   proximity_radius;
    int   door_tile_count;
    int   door_tiles[4][2];
    size_t property_count;
    tiled_property_t *properties;
} tiled_object_t;
