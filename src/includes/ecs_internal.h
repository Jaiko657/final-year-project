#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ecs.h"
#include "ecs_physics.h"
#include "dynarray.h"

// ===== Internal component storage types =====
typedef struct { float x, y; } cmp_position_t;
typedef struct { float x, y; smoothed_facing_t facing; } cmp_velocity_t;
typedef struct {
    ecs_entity_t target;
    float desired_distance;
    float max_speed;
    float vision_range;
    float last_seen_x;
    float last_seen_y;
    bool  has_last_seen;
} cmp_follow_t;

typedef struct {
    tex_handle_t tex;
    rectf src;
    float ox, oy;
} cmp_sprite_t;

typedef struct {
    int frame_w;
    int frame_h;

    int anim_count;
    const int* frames_per_anim;      // anim_count entries
    const int* anim_offsets;         // anim_count entries, prefix sum into frames
    const anim_frame_coord_t* frames; // flattened frames table

    int   current_anim;
    int   frame_index;
    float frame_duration;
    float current_time;
} cmp_anim_t;

typedef struct { float hx, hy; } cmp_collider_t;

typedef struct {
    float    pad;
    uint32_t target_mask;
} cmp_trigger_t;

typedef struct {
    char text[64];
    float y_offset;
    float linger;
    float timer;
    billboard_state_t state;
} cmp_billboard_t;

typedef struct {
    liftable_state_t state;
    ecs_entity_t carrier;
    float height;
    float vertical_velocity;
    float carry_height;
    float carry_distance;
    float pickup_distance;
    float pickup_radius;
    float throw_speed;
    float throw_vertical_speed;
    float gravity;
    float vx;
    float vy;
    float air_friction;
    float bounce_damping;
} cmp_liftable_t;

typedef struct {
    int x, y;
    int layer_idx;
    int tileset_idx;
    int base_tile_id;
    uint32_t flip_flags;
} door_tile_info_t;
typedef DA(door_tile_xy_t) door_tile_xy_list_t;
typedef DA(door_tile_info_t) door_tile_info_list_t;

typedef struct {
    float prox_radius;
    float prox_off_x;
    float prox_off_y;
    door_tile_info_list_t tiles;
    int current_frame;
    float anim_time_ms;
    bool intent_open;
    enum { DOOR_CLOSED = 0, DOOR_OPENING, DOOR_OPEN, DOOR_CLOSING } state;
} cmp_door_t;

// ===== Global ECS storage (defined in ecs_core.c) =====
extern uint32_t        ecs_mask[ECS_MAX_ENTITIES];
extern uint32_t        ecs_gen[ECS_MAX_ENTITIES];
extern uint32_t        ecs_next_gen[ECS_MAX_ENTITIES];
extern cmp_position_t  cmp_pos[ECS_MAX_ENTITIES];
extern cmp_velocity_t  cmp_vel[ECS_MAX_ENTITIES];
extern cmp_follow_t    cmp_follow[ECS_MAX_ENTITIES];
extern cmp_anim_t      cmp_anim[ECS_MAX_ENTITIES];
extern cmp_sprite_t    cmp_spr[ECS_MAX_ENTITIES];
extern cmp_collider_t  cmp_col[ECS_MAX_ENTITIES];
extern cmp_trigger_t   cmp_trigger[ECS_MAX_ENTITIES];
extern cmp_billboard_t cmp_billboard[ECS_MAX_ENTITIES];
extern cmp_phys_body_t cmp_phys_body[ECS_MAX_ENTITIES];
extern cmp_liftable_t  cmp_liftable[ECS_MAX_ENTITIES];
extern cmp_door_t      cmp_door[ECS_MAX_ENTITIES];

// ===== Internal helpers =====
int  ent_index_checked(ecs_entity_t e);
int  ent_index_unchecked(ecs_entity_t e);
bool ecs_alive_idx(int i);
bool ecs_alive_handle(ecs_entity_t e);
ecs_entity_t handle_from_index(int i);
float clampf(float v, float a, float b);

ecs_entity_t find_player_handle(void);

// Toast (TODO: move to engine component when engine exists)
void ui_toast(float secs, const char* fmt, ...);

// Anim allocator lifecycle (arena-backed animation data)
void ecs_anim_reset_allocator(void);
void ecs_anim_shutdown_allocator(void);
