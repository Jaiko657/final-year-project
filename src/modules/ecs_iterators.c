#include "../includes/ecs_internal.h"
#include "../includes/logger.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// --- SPRITES ---
ecs_sprite_iter_t ecs_sprites_begin(void) { return (ecs_sprite_iter_t){ .i = -1 }; }

bool ecs_sprites_next(ecs_sprite_iter_t* it, ecs_sprite_view_t* out)
{
    for (int i = it->i + 1; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & (CMP_POS | CMP_SPR)) != (CMP_POS | CMP_SPR)) continue;

        it->i = i;
        *out = (ecs_sprite_view_t){
            .tex = cmp_spr[i].tex,
            .src = cmp_spr[i].src,
            .x   = cmp_pos[i].x,
            .y   = cmp_pos[i].y,
            .ox  = cmp_spr[i].ox,
            .oy  = cmp_spr[i].oy,
        };
        return true;
    }
    return false;
}

// --- COLLIDERS ---
ecs_collider_iter_t ecs_colliders_begin(void) { return (ecs_collider_iter_t){ .i = -1 }; }

bool ecs_colliders_next(ecs_collider_iter_t* it, ecs_collider_view_t* out)
{
    for (int i = it->i + 1; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & (CMP_POS | CMP_COL)) != (CMP_POS | CMP_COL)) continue;

        it->i = i;
        *out = (ecs_collider_view_t){
            .x = cmp_pos[i].x, .y = cmp_pos[i].y,
            .hx = cmp_col[i].hx, .hy = cmp_col[i].hy,
        };
        return true;
    }
    return false;
}

// --- TRIGGERS ---
ecs_trigger_iter_t ecs_triggers_begin(void) { return (ecs_trigger_iter_t){ .i = -1 }; }

bool ecs_triggers_next(ecs_trigger_iter_t* it, ecs_trigger_view_t* out)
{
    for (int i = it->i + 1; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & (CMP_POS | CMP_TRIGGER)) != (CMP_POS | CMP_TRIGGER)) continue;

        it->i = i;

        float collider_hx = 0.0f;
        float collider_hy = 0.0f;
        if(ecs_mask[i] & CMP_COL) {
            collider_hx = cmp_col[i].hx;
            collider_hy = cmp_col[i].hy;
        }

        *out = (ecs_trigger_view_t){
            .x   = cmp_pos[i].x,
            .y   = cmp_pos[i].y,
            .hx  = collider_hx,
            .hy  = collider_hy,
            .pad = cmp_trigger[i].pad,
        };
        return true;
    }
    return false;
}

// --- BILLBOARDS ---
ecs_billboard_iter_t ecs_billboards_begin(void) { return (ecs_billboard_iter_t){ .i = -1 }; }

bool ecs_billboards_next(ecs_billboard_iter_t* it, ecs_billboard_view_t* out)
{
    for (int i = it->i + 1; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & (CMP_POS|CMP_BILLBOARD)) != (CMP_POS|CMP_BILLBOARD)) continue;
        if (cmp_billboard[i].state != BILLBOARD_ACTIVE) continue;
        if (cmp_billboard[i].timer <= 0.0f) continue;

        it->i = i;

        float a = 1.0f;
        if (cmp_billboard[i].linger > 0.0f) {
            a = clampf(cmp_billboard[i].timer / cmp_billboard[i].linger, 0.0f, 1.0f);
        }

        *out = (ecs_billboard_view_t){
            .x        = cmp_pos[i].x,
            .y        = cmp_pos[i].y,
            .y_offset = cmp_billboard[i].y_offset,
            .alpha    = a,
            .text     = cmp_billboard[i].text
        };
        return true;
    }
    return false;
}
