#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "modules/ecs/ecs.h"
#include "modules/ecs/ecs_physics_types.h"
#include "modules/common/resource_handles.h"

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
    bool highlighted;
    colorf highlight_color;
    int highlight_thickness;
} cmp_sprite_fx_t;

typedef struct {
    tex_handle_t tex;
    rectf src;
    float ox, oy;
    cmp_sprite_fx_t fx;
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
    grav_gun_state_t state;
    ecs_entity_t holder;
    float pickup_distance;
    float pickup_radius;
    float max_hold_distance;
    float breakoff_distance;
    float follow_gain;
    float max_speed;
    float damping;
    float hold_vel_x;
    float hold_vel_y;
    float grab_offset_x;
    float grab_offset_y;
    unsigned int saved_mask_bits;
    bool saved_mask_valid;
} cmp_grav_gun_t;

typedef struct {
    float prox_radius;
    world_door_handle_t world_handle;
    enum { DOOR_CLOSED = 0, DOOR_OPENING, DOOR_OPEN, DOOR_CLOSING } state;
    float anim_time_ms;
    bool intent_open;
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
extern cmp_grav_gun_t  cmp_grav_gun[ECS_MAX_ENTITIES];
extern cmp_door_t      cmp_door[ECS_MAX_ENTITIES];

// ===== Internal helpers =====
int  ent_index_checked(ecs_entity_t e);
int  ent_index_unchecked(ecs_entity_t e);
bool ecs_alive_idx(int i);
bool ecs_alive_handle(ecs_entity_t e);
ecs_entity_t handle_from_index(int i);
float clampf(float v, float a, float b);

ecs_entity_t find_player_handle(void);

// Anim allocator lifecycle (arena-backed animation data)
void ecs_anim_reset_allocator(void);
void ecs_anim_shutdown_allocator(void);

// Door component lifecycle (world-owned registration)
void ecs_door_on_destroy(int idx);

// Component lifecycle hooks (registered by engine/gameplay modules).
typedef void (*ecs_component_hook_fn)(int idx);
void ecs_register_component_destroy_hook(ComponentEnum comp, ecs_component_hook_fn fn);
void ecs_register_phys_body_create_hook(ecs_component_hook_fn fn);
void ecs_register_render_component_hooks(void);
void ecs_register_physics_component_hooks(void);
void ecs_register_door_component_hooks(void);
