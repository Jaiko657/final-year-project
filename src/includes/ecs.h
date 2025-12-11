#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "engine_types.h"
#include "input.h"
#include "asset.h"
#include "ecs_systems.h"
#include "ecs_physics.h"
// TODO: ANIMATION SYSTEM IMPROVMENTS: locality bad, and fixed size bugs
#define MAX_ANIMS  16
#define MAX_FRAMES 16

//LOAD IN COMPONENTS (might revert this but worried about using same mask 2 times and not realising)
typedef enum {
#define X(name) ENUM_##name,
    #include "components.def"
#undef X
    ENUM_COMPONENT_COUNT
} ComponentEnum;
enum {
#define X(name) CMP_##name = 1u << ENUM_##name,
    #include "components.def"
#undef X
};

// --- draw views (data only) ---
typedef struct {
    tex_handle_t tex;
    rectf src;
    float x, y;   // world position
    float ox, oy; // origin
} ecs_sprite_view_t;

typedef struct {
    float ecs_x, ecs_y;   // ECS transform center
    float phys_x, phys_y; // Chipmunk body center (falls back to ECS if none)
    float hx, hy;         // half extents
    bool  has_phys;       // true if Chipmunk body is valid
} ecs_collider_view_t;

typedef struct {
    float x, y; // center
    float hx, hy; // half extents
    float pad;    // AABB padding from collider
} ecs_trigger_view_t;

typedef struct {
    int count[100]; // if I use this more than 100 times I'm an idiot
    int num;
} ecs_count_result_t; //For debug data passing

// ====== Public constants / types ======
#define ECS_MAX_ENTITIES 1024

typedef struct {
    uint32_t idx;
    uint32_t gen;     // generation (0 == dead)
} ecs_entity_t;

static inline ecs_entity_t ecs_null(void){ return (ecs_entity_t){UINT32_MAX,0}; }

// facing / animation helpers
typedef enum {
    DIR_NORTH = 0,
    DIR_NORTHEAST,
    DIR_EAST,
    DIR_SOUTHEAST,
    DIR_SOUTH,
    DIR_SOUTHWEST,
    DIR_WEST,
    DIR_NORTHWEST,
} facing_t;

typedef struct {
    facing_t rawDir;
    facing_t facingDir;
    facing_t candidateDir;
    float candidateTime;
} smoothed_facing_t;

typedef struct {
    uint8_t col;
    uint8_t row;
} anim_frame_coord_t;

typedef enum { BILLBOARD_INACTIVE = 0, BILLBOARD_ACTIVE = 1 } billboard_state_t;

typedef enum {
    LIFTABLE_STATE_ONGROUND = 0,
    LIFTABLE_STATE_CARRIED  = 1,
    LIFTABLE_STATE_THROWN   = 2
} liftable_state_t;

typedef struct {
    float x, y;
    float y_offset;
    float alpha;
    const char* text;
} ecs_billboard_view_t;

typedef struct { int i; } ecs_billboard_iter_t;

// ====== Lifecycle / config ======
void ecs_init(void);
void ecs_shutdown(void);
void ecs_set_world_size(int w, int h);
void ecs_get_world_size(int* out_w, int* out_h);
bool ecs_get_player_position(float* out_x, float* out_y);
bool ecs_get_position(ecs_entity_t e, v2f* out_pos);
ecs_entity_t ecs_find_player(void);

// ====== Entity / components ======
ecs_entity_t ecs_create(void);
void         ecs_destroy(ecs_entity_t e);

void cmp_add_position (ecs_entity_t e, float x, float y);
void cmp_add_velocity(ecs_entity_t e, float x, float y, facing_t direction);
void cmp_add_sprite_handle(ecs_entity_t e, tex_handle_t h, rectf src, float ox, float oy);
void cmp_add_sprite_path  (ecs_entity_t e, const char* path, rectf src, float ox, float oy);
void cmp_add_anim(
    ecs_entity_t e,
    int frame_w,
    int frame_h,
    int anim_count,
    const int* frames_per_anim,
    const anim_frame_coord_t frames[][MAX_FRAMES],
    float fps);
void cmp_add_player   (ecs_entity_t e);
void cmp_add_follow   (ecs_entity_t e, ecs_entity_t target, float desired_distance, float max_speed);
void cmp_add_trigger  (ecs_entity_t e, float pad, uint32_t target_mask);
void cmp_add_billboard(ecs_entity_t e, const char* text, float y_off, float linger, billboard_state_t state);
void cmp_add_size     (ecs_entity_t e, float hx, float hy); // AABB half-extents
void cmp_add_liftable (ecs_entity_t e);
void cmp_add_phys_body(ecs_entity_t e, PhysicsType type, float mass, float restitution, float friction);
void cmp_add_phys_body_default(ecs_entity_t e, PhysicsType type);
void cmp_add_door(ecs_entity_t e, float prox_radius, float prox_off_x, float prox_off_y, int tile_count, const int (*tile_xy)[2]);

// ====== Update & render ======
void ecs_tick(float fixed_dt, const input_t* in);
void ecs_present(float fixed_dt);

// ========= Iterators  ========
// --- sprite iterator ---
typedef struct { int i; } ecs_sprite_iter_t;
ecs_sprite_iter_t ecs_sprites_begin(void);
bool ecs_sprites_next(ecs_sprite_iter_t* it, ecs_sprite_view_t* out);

// --- collider iterator ---
typedef struct { int i; } ecs_collider_iter_t;
ecs_collider_iter_t ecs_colliders_begin(void);
bool ecs_colliders_next(ecs_collider_iter_t* it, ecs_collider_view_t* out);

// --- trigger iterator ---
typedef struct { int i; } ecs_trigger_iter_t;
ecs_trigger_iter_t ecs_triggers_begin(void);
bool ecs_triggers_next(ecs_trigger_iter_t* it, ecs_trigger_view_t* out);

// --- billboard iterator ---
ecs_billboard_iter_t ecs_billboards_begin(void);
bool ecs_billboards_next(ecs_billboard_iter_t* it, ecs_billboard_view_t* out);

// ====== HUD helpers ======
ecs_count_result_t ecs_count_entities(const uint32_t* masks, int num_masks);

// toast
bool        ecs_toast_is_active(void);
const char* ecs_toast_get_text(void);
