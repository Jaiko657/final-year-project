#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "raylib.h"             // Texture2D, Rectangle
#include "input.h"

// ====== Public constants / types ======
#define ECS_MAX_ENTITIES 1024
typedef struct {
    uint32_t idx;
    uint32_t gen;     // generation (0 == dead)
} ecs_entity_t;
static inline ecs_entity_t ecs_null(void){ return (ecs_entity_t){UINT32_MAX,0}; }

typedef enum { ITEM_COIN = 1, ITEM_HAT = 2 } item_kind_t;

// ====== Lifecycle / config ======
void ecs_init(void);
void ecs_shutdown(void);
void ecs_set_world_size(int w, int h);
void ecs_set_hat_texture(Texture2D tex);

// ====== Entity / components ======
ecs_entity_t ecs_create(void);
void         ecs_destroy(ecs_entity_t e);

void cmp_add_position (ecs_entity_t e, float x, float y);
void cmp_add_velocity (ecs_entity_t e, float x, float y);
void cmp_add_sprite   (ecs_entity_t e, Texture2D t, Rectangle src, float ox, float oy);
void tag_add_player   (ecs_entity_t e);
void cmp_add_inventory(ecs_entity_t e);
void cmp_add_item     (ecs_entity_t e, item_kind_t k);
void cmp_add_vendor   (ecs_entity_t e, item_kind_t sells, int price);
void cmp_add_size     (ecs_entity_t e, float hx, float hy); // AABB half-extents

// ====== Update & render ======
void ecs_tick(float fixed_dt, const input_t* in);
void ecs_render_world(void);
void ecs_debug_draw(void);
void ecs_draw_vendor_hints(void);

// ====== HUD helpers ======
void ecs_get_player_stats(int* outCoins, bool* outHasHat);
