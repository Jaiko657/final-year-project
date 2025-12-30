#pragma once

#include "modules/ecs/ecs.h"
#include "modules/asset/asset.h"

// --- draw views (data only) ---
typedef struct {
    tex_handle_t tex;
    rectf src;
    float x, y;   // world position
    float ox, oy; // origin
} ecs_sprite_view_t;

typedef struct {
    float ecs_x, ecs_y;   // ECS transform center
    float phys_x, phys_y; // Physics body center (falls back to ECS if none)
    float hx, hy;         // half extents
    bool  has_phys;       // true if physics body is active
} ecs_collider_view_t;

typedef struct {
    float x, y; // center
    float hx, hy; // half extents
    float pad;    // AABB padding from collider
} ecs_trigger_view_t;

typedef struct {
    float x, y;
    float y_offset;
    float alpha;
    const char* text;
} ecs_billboard_view_t;

// Sprite component helpers (render/asset-facing).
void cmp_add_sprite_handle(ecs_entity_t e, tex_handle_t h, rectf src, float ox, float oy);
void cmp_add_sprite_path  (ecs_entity_t e, const char* path, rectf src, float ox, float oy);

// ========= Iterators (render-facing) ========
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
typedef struct { int i; } ecs_billboard_iter_t;
ecs_billboard_iter_t ecs_billboards_begin(void);
bool ecs_billboards_next(ecs_billboard_iter_t* it, ecs_billboard_view_t* out);
