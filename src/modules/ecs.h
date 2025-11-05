#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../includes/engine_types.h"
#include "input.h"
#include "asset.h"
#include "ecs_systems.h"

// =============== Bitmasks / Tags =========
#define CMP_POS       (1<<0)
#define CMP_VEL       (1<<1)
#define CMP_SPR       (1<<2)
#define TAG_PLAYER    (1<<4)
#define CMP_ITEM      (1<<5)
#define CMP_INV       (1<<6)
#define CMP_VENDOR    (1<<7)
#define CMP_COL       (1<<8)
#define CMP_TRIGGER   (1<<9)
#define CMP_BILLBOARD (1<<10)

// --- draw views (data only) ---
typedef struct {
    tex_handle_t tex;
    rectf src;
    float x, y;   // world position
    float ox, oy; // origin
} ecs_sprite_view_t;

typedef struct {
    float x, y; // center
    float hx, hy; // half extents
} ecs_collider_view_t;

// ====== Public constants / types ======
#define ECS_MAX_ENTITIES 1024
typedef struct {
    uint32_t idx;
    uint32_t gen;     // generation (0 == dead)
} ecs_entity_t;
static inline ecs_entity_t ecs_null(void){ return (ecs_entity_t){UINT32_MAX,0}; }

typedef struct {
    ecs_entity_t a;   // the entity that owns CMP_TRIGGER
    ecs_entity_t b;   // the entity that matched filter & is within pad
} prox_pair_t;

typedef enum { BILLBOARD_INACTIVE = 0, BILLBOARD_ACTIVE = 1} billboard_state_t;
typedef struct { char text[64]; float y_offset, linger, timer; billboard_state_t state; } cmp_billboard_t;

typedef enum { ITEM_COIN = 1, ITEM_HAT = 2 } item_kind_t;

// ====== Lifecycle / config ======
void ecs_init(void);
void ecs_shutdown(void);
void ecs_set_world_size(int w, int h);

// ====== Entity / components ======
ecs_entity_t ecs_create(void);
void         ecs_destroy(ecs_entity_t e);

void cmp_add_position (ecs_entity_t e, float x, float y);
void cmp_add_velocity (ecs_entity_t e, float x, float y);
void cmp_add_sprite_handle(ecs_entity_t e, tex_handle_t h, rectf src, float ox, float oy);
void cmp_add_sprite_path  (ecs_entity_t e, const char* path, rectf src, float ox, float oy);
void tag_add_player   (ecs_entity_t e);
void cmp_add_trigger(ecs_entity_t e, float pad, uint32_t target_mask);
void cmp_add_billboard(ecs_entity_t e, const char* text, float y_off, float linger, billboard_state_t state);
void cmp_add_inventory(ecs_entity_t e);
void cmp_add_item     (ecs_entity_t e, item_kind_t k);
void cmp_add_vendor   (ecs_entity_t e, item_kind_t sells, int price);
void cmp_add_size     (ecs_entity_t e, float hx, float hy); // AABB half-extents

// ====== Update & render ======
void ecs_tick(float fixed_dt, const input_t* in);

// ========= Iterators  ========
// --- sprite iterator ---
typedef struct { int i; } ecs_sprite_iter_t;
ecs_sprite_iter_t ecs_sprites_begin(void);
bool ecs_sprites_next(ecs_sprite_iter_t* it, ecs_sprite_view_t* out);

// --- collider iterator ---
typedef struct { int i; } ecs_collider_iter_t;
ecs_collider_iter_t ecs_colliders_begin(void);
bool ecs_colliders_next(ecs_collider_iter_t* it, ecs_collider_view_t* out);

typedef struct { int i; } ecs_billboard_iter_t;
typedef struct { float x,y, y_offset, alpha; const char* text; } ecs_billboard_view_t;
ecs_billboard_iter_t ecs_billboards_begin(void);
bool ecs_billboards_next(ecs_billboard_iter_t* it, ecs_billboard_view_t* out);

// ====== HUD helpers ======
void ecs_get_player_stats(int* outCoins, bool* outHasHat);
// toast
bool        ecs_toast_is_active(void);
const char* ecs_toast_get_text(void);

// vendor hint: world anchor + message if active
bool ecs_vendor_hint_is_active(int* out_x, int* out_y, const char** out_text);
