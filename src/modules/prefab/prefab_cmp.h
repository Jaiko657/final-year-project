#pragma once

// Public API: prefab component parsers ("build" functions).
// These functions are pure parsing: they fill out-structs and do not touch ECS state.

#include <stdbool.h>
#include <stdint.h>

#include "modules/common/dynarray.h"
#include "modules/ecs/ecs.h"
#include "modules/ecs/ecs_game.h"
#include "modules/world/door_tiles.h"
#include "modules/prefab/prefab.h"
#include "modules/tiled/tiled.h"

// Shared prefab parsing helpers (implemented in `prefab_cmp_common.c`)
uint32_t prefab_parse_mask(const char* s, bool* out_ok);
bool     prefab_parse_float(const char* s, float* out_v);
bool     prefab_parse_int(const char* s, int* out_v);
facing_t prefab_parse_facing(const char* s, facing_t fallback);

const char* prefab_find_prop(const prefab_component_t* comp, const char* field);
const char* prefab_override_value(const prefab_component_t* comp, const tiled_object_t* obj, const char* field);
const char* prefab_combined_value(const prefab_component_t* comp, const tiled_object_t* obj, const char* field);

v2f prefab_object_position_default(const tiled_object_t* obj);

typedef struct {
    float x, y;
} prefab_cmp_pos_t;
bool prefab_cmp_pos_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_pos_t* out_pos);

typedef struct {
    float x, y;
    facing_t dir;
} prefab_cmp_vel_t;
bool prefab_cmp_vel_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_vel_t* out_vel);

typedef struct {
    PhysicsType type;
    float mass;
} prefab_cmp_phys_body_t;
bool prefab_cmp_phys_body_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_phys_body_t* out_body);

typedef struct {
    const char* path;
    rectf src;
    float ox, oy;
} prefab_cmp_spr_t;
bool prefab_cmp_spr_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_spr_t* out_spr);

typedef struct {
    int frame_w;
    int frame_h;
    float fps;
    int anim_count;
    int frame_buffer_width;
    int frame_buffer_height;
    int* frames_per_anim;
    anim_frame_coord_t* frames;
} prefab_cmp_anim_t;

static inline bool prefab_cmp_anim_frame_coord_valid(const prefab_cmp_anim_t* anim, int seq, int frame)
{
    if (!anim) return false;
    if (anim->frame_buffer_height <= 0 || anim->frame_buffer_width <= 0) return false;
    if (!anim->frames) return false;
    if (seq < 0 || seq >= anim->frame_buffer_height) return false;
    if (frame < 0 || frame >= anim->frame_buffer_width) return false;
    return true;
}

static inline anim_frame_coord_t* prefab_cmp_anim_frame_coord_mut(prefab_cmp_anim_t* anim, int seq, int frame)
{
    if (!prefab_cmp_anim_frame_coord_valid(anim, seq, frame)) return NULL;
    int offset = seq * anim->frame_buffer_width + frame;
    return &anim->frames[offset];
}

static inline const anim_frame_coord_t* prefab_cmp_anim_frame_coord(const prefab_cmp_anim_t* anim, int seq, int frame)
{
    if (!prefab_cmp_anim_frame_coord_valid(anim, seq, frame)) return NULL;
    int offset = seq * anim->frame_buffer_width + frame;
    return &anim->frames[offset];
}

bool prefab_cmp_anim_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_anim_t* out_anim);
void prefab_cmp_anim_free(prefab_cmp_anim_t* anim);

typedef struct { item_kind_t kind; } prefab_cmp_item_t;
bool prefab_cmp_item_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_item_t* out_item);

typedef struct { item_kind_t sells; int price; } prefab_cmp_vendor_t;
bool prefab_cmp_vendor_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_vendor_t* out_vendor);

typedef enum {
    PREFAB_FOLLOW_TARGET_NONE = 0,
    PREFAB_FOLLOW_TARGET_PLAYER,
    PREFAB_FOLLOW_TARGET_ENTITY_IDX,
} prefab_follow_target_kind_t;

typedef struct {
    prefab_follow_target_kind_t target_kind;
    uint32_t target_idx; // only valid when target_kind == PREFAB_FOLLOW_TARGET_ENTITY_IDX
    float desired_distance;
    float max_speed;
    bool have_vision_range;
    float vision_range;
} prefab_cmp_follow_t;
bool prefab_cmp_follow_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_follow_t* out_follow);

typedef struct { float hx, hy; } prefab_cmp_col_t;
bool prefab_cmp_col_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_col_t* out_col);

typedef struct { float pad; uint32_t target_mask; } prefab_cmp_trigger_t;
bool prefab_cmp_trigger_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_trigger_t* out_trigger);

typedef struct {
    const char* text;
    float y_offset;
    float linger;
    billboard_state_t state;
} prefab_cmp_billboard_t;
bool prefab_cmp_billboard_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_billboard_t* out_billboard);

typedef struct {
    bool has_pickup_distance; float pickup_distance;
    bool has_pickup_radius; float pickup_radius;
    bool has_max_hold_distance; float max_hold_distance;
    bool has_breakoff_distance; float breakoff_distance;
    bool has_follow_gain; float follow_gain;
    bool has_max_speed; float max_speed;
    bool has_damping; float damping;
} prefab_cmp_grav_gun_t;
bool prefab_cmp_grav_gun_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_grav_gun_t* out_grav_gun);

typedef DA(door_tile_xy_t) prefab_door_tile_xy_list_t;

typedef struct {
    float prox_radius;
    prefab_door_tile_xy_list_t tiles;
} prefab_cmp_door_t;
bool prefab_cmp_door_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_door_t* out_door);
void prefab_cmp_door_free(prefab_cmp_door_t* door);
