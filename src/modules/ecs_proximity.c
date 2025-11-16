#include "../includes/ecs_internal.h"
#include "../includes/ecs_proximity.h"
#include <math.h>
#include <string.h>

// =============== Proximity View (transient each tick) =============
#define PROX_MAX ECS_MAX_ENTITIES

static ecs_prox_view_t prox_curr[PROX_MAX];
static ecs_prox_view_t prox_prev[PROX_MAX];
static int             prox_curr_count = 0;
static int             prox_prev_count = 0;

static bool prox_contains(ecs_prox_view_t* arr, int n, ecs_prox_view_t p){
    for (int i=0;i<n;++i){
        if (arr[i].trigger_owner.idx==p.trigger_owner.idx && arr[i].trigger_owner.gen==p.trigger_owner.gen &&
            arr[i].matched_entity.idx==p.matched_entity.idx && arr[i].matched_entity.gen==p.matched_entity.gen) return true;
    }
    return false;
}

// public iterators
ecs_prox_iter_t ecs_prox_stay_begin(void){ return (ecs_prox_iter_t){ .i = -1 }; }

bool ecs_prox_stay_next(ecs_prox_iter_t* it, ecs_prox_view_t* out){
    int i = it->i + 1;
    while (i < prox_curr_count){
        if (ecs_alive_handle(prox_curr[i].trigger_owner) && ecs_alive_handle(prox_curr[i].matched_entity)){
            it->i = i;
            *out = prox_curr[i];
            return true;
        }
        ++i;
    }
    return false;
}

ecs_prox_iter_t ecs_prox_enter_begin(void){ return (ecs_prox_iter_t){ .i = -1 }; }

bool ecs_prox_enter_next(ecs_prox_iter_t* it, ecs_prox_view_t* out){
    for (int i = it->i + 1; i < prox_curr_count; ++i){
        if (!ecs_alive_handle(prox_curr[i].trigger_owner) || !ecs_alive_handle(prox_curr[i].matched_entity)) continue;
        if (!prox_contains(prox_prev, prox_prev_count, prox_curr[i])){
            it->i = i;
            *out = prox_curr[i];
            return true;
        }
    }
    return false;
}

ecs_prox_iter_t ecs_prox_exit_begin(void){ return (ecs_prox_iter_t){ .i = -1 }; }

bool ecs_prox_exit_next(ecs_prox_iter_t* it, ecs_prox_view_t* out){
    for (int i = it->i + 1; i < prox_prev_count; ++i){
        if (!ecs_alive_handle(prox_prev[i].trigger_owner) || !ecs_alive_handle(prox_prev[i].matched_entity)) continue;
        if (!prox_contains(prox_curr, prox_curr_count, prox_prev[i])){
            it->i = i;
            *out = prox_prev[i];
            return true;
        }
    }
    return false;
}

static bool col_overlap_padded_idx(int a, int b, float pad){
    float ax = cmp_pos[a].x, ay = cmp_pos[a].y;
    float bx = cmp_pos[b].x, by = cmp_pos[b].y;

    float ahx = (ecs_mask[a] & CMP_COL) ? (cmp_col[a].hx + pad) : pad;
    float ahy = (ecs_mask[a] & CMP_COL) ? (cmp_col[a].hy + pad) : pad;
    float bhx = (ecs_mask[b] & CMP_COL) ?  cmp_col[b].hx : 0.f;
    float bhy = (ecs_mask[b] & CMP_COL) ?  cmp_col[b].hy : 0.f;

    return fabsf(ax - bx) <= (ahx + bhx) && fabsf(ay - by) <= (ahy + bhy);
}

// ---- systems ----
static void sys_proximity_build_view_impl(void)
{
    if (prox_curr_count > 0) {
        memcpy(prox_prev, prox_curr, sizeof(ecs_prox_view_t)*prox_curr_count);
    }
    prox_prev_count = prox_curr_count;
    prox_curr_count = 0;

    for (int a=0; a<ECS_MAX_ENTITIES; ++a){
        if(!ecs_alive_idx(a)) continue;
        if ((ecs_mask[a] & (CMP_POS|CMP_COL|CMP_TRIGGER)) != (CMP_POS|CMP_COL|CMP_TRIGGER)) continue;

        const cmp_trigger_t* tr = &cmp_trigger[a];

        for (int b=0; b<ECS_MAX_ENTITIES; ++b) {
            if (b==a || !ecs_alive_idx(b)) continue;
            if ((ecs_mask[b] & tr->target_mask) != tr->target_mask) continue;
            if ((ecs_mask[b] & (CMP_POS|CMP_COL)) != (CMP_POS|CMP_COL)) continue;

            if (col_overlap_padded_idx(a, b, tr->pad)){
                if (prox_curr_count < PROX_MAX){
                    prox_curr[prox_curr_count++] =
                        (ecs_prox_view_t){ handle_from_index(a), handle_from_index(b) };
                }
            }
        }
    }
}

static void sys_billboards_impl(float dt)
{
    for (int i=0;i<ECS_MAX_ENTITIES;++i){
        if (!ecs_alive_idx(i) || !(ecs_mask[i]&CMP_BILLBOARD)) continue;
        if (cmp_billboard[i].timer > 0) {
            cmp_billboard[i].timer -= dt;
            if (cmp_billboard[i].timer < 0) cmp_billboard[i].timer = 0;
        }
    }

    ecs_prox_iter_t it = ecs_prox_stay_begin();
    ecs_prox_view_t v;
    while (ecs_prox_stay_next(&it, &v)){
        int a = ent_index_checked(v.trigger_owner);
        if (a >= 0 && (ecs_mask[a] & CMP_BILLBOARD)){
            cmp_billboard[a].timer = cmp_billboard[a].linger;
        }
    }
}

// Public adapters (used by ecs_core.c)
void sys_prox_build_adapt(float dt, const input_t* in)
{
    (void)dt; (void)in;
    sys_proximity_build_view_impl();
}

void sys_billboards_adapt(float dt, const input_t* in)
{
    (void)in;
    sys_billboards_impl(dt);
}
