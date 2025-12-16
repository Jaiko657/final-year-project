#include "../includes/ecs_internal.h"
#include "../includes/ecs_proximity.h"
#include "../includes/dynarray.h"
#include <math.h>
#include <string.h>

// =============== Proximity View (transient each tick) =============
static DA(ecs_prox_view_t) prox_curr = {0};
static DA(ecs_prox_view_t) prox_prev = {0};

static bool prox_contains(const ecs_prox_view_t* arr, int n, ecs_prox_view_t p){
    for (int i = 0; i < n; ++i){
        if (arr[i].trigger_owner.idx==p.trigger_owner.idx && arr[i].trigger_owner.gen==p.trigger_owner.gen &&
            arr[i].matched_entity.idx==p.matched_entity.idx && arr[i].matched_entity.gen==p.matched_entity.gen) return true;
    }
    return false;
}

// public iterators
ecs_prox_iter_t ecs_prox_stay_begin(void){ return (ecs_prox_iter_t){ .i = -1 }; }

bool ecs_prox_stay_next(ecs_prox_iter_t* it, ecs_prox_view_t* out){
    int count = (int)prox_curr.size;
    int i = it->i + 1;
    while (i < count){
        if (ecs_alive_handle(prox_curr.data[i].trigger_owner) && ecs_alive_handle(prox_curr.data[i].matched_entity)){
            it->i = i;
            *out = prox_curr.data[i];
            return true;
        }
        ++i;
    }
    return false;
}

ecs_prox_iter_t ecs_prox_enter_begin(void){ return (ecs_prox_iter_t){ .i = -1 }; }

bool ecs_prox_enter_next(ecs_prox_iter_t* it, ecs_prox_view_t* out){
    int curr_count = (int)prox_curr.size;
    int prev_count = (int)prox_prev.size;
    for (int i = it->i + 1; i < curr_count; ++i){
        if (!ecs_alive_handle(prox_curr.data[i].trigger_owner) || !ecs_alive_handle(prox_curr.data[i].matched_entity)) continue;
        if (!prox_contains(prox_prev.data, prev_count, prox_curr.data[i])){
            it->i = i;
            *out = prox_curr.data[i];
            return true;
        }
    }
    return false;
}

ecs_prox_iter_t ecs_prox_exit_begin(void){ return (ecs_prox_iter_t){ .i = -1 }; }

bool ecs_prox_exit_next(ecs_prox_iter_t* it, ecs_prox_view_t* out){
    int prev_count = (int)prox_prev.size;
    int curr_count = (int)prox_curr.size;
    for (int i = it->i + 1; i < prev_count; ++i){
        if (!ecs_alive_handle(prox_prev.data[i].trigger_owner) || !ecs_alive_handle(prox_prev.data[i].matched_entity)) continue;
        if (!prox_contains(prox_curr.data, curr_count, prox_prev.data[i])){
            it->i = i;
            *out = prox_prev.data[i];
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
    if (prox_curr.size > 0) {
        DA_RESERVE(&prox_prev, prox_curr.size);
        memcpy(prox_prev.data, prox_curr.data, sizeof(ecs_prox_view_t) * prox_curr.size);
    }
    prox_prev.size = prox_curr.size;
    DA_CLEAR(&prox_curr);

    for (int a=0; a<ECS_MAX_ENTITIES; ++a){
        if(!ecs_alive_idx(a)) continue;
        if ((ecs_mask[a] & (CMP_POS|CMP_COL|CMP_TRIGGER)) != (CMP_POS|CMP_COL|CMP_TRIGGER)) continue;

        const cmp_trigger_t* tr = &cmp_trigger[a];

        for (int b=0; b<ECS_MAX_ENTITIES; ++b) {
            if (b==a || !ecs_alive_idx(b)) continue;
            if ((ecs_mask[b] & tr->target_mask) != tr->target_mask) continue;
            if ((ecs_mask[b] & (CMP_POS|CMP_COL)) != (CMP_POS|CMP_COL)) continue;

            if (col_overlap_padded_idx(a, b, tr->pad)){
                ecs_prox_view_t v = { handle_from_index(a), handle_from_index(b) };
                DA_APPEND(&prox_curr, v);
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
