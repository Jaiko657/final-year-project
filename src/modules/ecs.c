#include "ecs.h"
#include "logger.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/*
 * TODO:
 * - registration of systems etc (modularity)
 * - duplication of size in sprite and collider?
*/
// TODO: FIX THE ORDERING OF THIS SHIT
static inline bool         ecs_alive_handle(ecs_entity_t e);
static inline ecs_entity_t handle_from_index(int i);
static inline int          ent_index_checked(ecs_entity_t e);

// =============== Components (internal) ===
typedef struct { float x, y; } cmp_position_t;
typedef struct { float x, y; } cmp_velocity_t;
typedef struct { tex_handle_t tex; rectf src; float ox, oy; } cmp_sprite_t;
typedef struct { float hx, hy; } cmp_collider_t;

typedef struct {
    float    pad;
    uint32_t target_mask;
} cmp_trigger_t;

// temp gameplay components kept for now
typedef struct { item_kind_t kind; } cmp_item_t;
typedef struct { int coins; bool hasHat; } cmp_inventory_t;
typedef struct { item_kind_t sells; int price; } cmp_vendor_t;

// =============== ECS Storage =============
static uint32_t        ecs_mask[ECS_MAX_ENTITIES];
static uint32_t        ecs_gen[ECS_MAX_ENTITIES];
static uint32_t        ecs_next_gen[ECS_MAX_ENTITIES];
static cmp_position_t  cmp_pos[ECS_MAX_ENTITIES];
static cmp_velocity_t  cmp_vel[ECS_MAX_ENTITIES];
static cmp_sprite_t    cmp_spr[ECS_MAX_ENTITIES];
static cmp_collider_t  cmp_col[ECS_MAX_ENTITIES];
static cmp_trigger_t   cmp_trigger[ECS_MAX_ENTITIES];
static cmp_billboard_t cmp_billboard[ECS_MAX_ENTITIES];
static cmp_item_t      cmp_item[ECS_MAX_ENTITIES];
static cmp_inventory_t cmp_inv[ECS_MAX_ENTITIES];
static cmp_vendor_t    cmp_vendor[ECS_MAX_ENTITIES];

// ========== O(1) create/delete ==========
static int      free_stack[ECS_MAX_ENTITIES];
static int      free_top = 0;

// Config
static int g_worldW = 800, g_worldH = 450;

// =============== Toast UI =================
static char  ui_toast_text[128] = {0};
static float ui_toast_timer = 0.0f;

static void ui_toast(float secs, const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    vsnprintf(ui_toast_text, sizeof(ui_toast_text), fmt, ap);
    va_end(ap);
    ui_toast_timer = secs;
}

// =============== Proximity View (transient each tick) =============
#define PROX_MAX ECS_MAX_ENTITIES

static prox_pair_t prox_curr[PROX_MAX];
static prox_pair_t prox_prev[PROX_MAX];
static int         prox_curr_count = 0;
static int         prox_prev_count = 0;

static inline bool prox_pair_eq(prox_pair_t x, prox_pair_t y){
    return x.a.idx==y.a.idx && x.a.gen==y.a.gen &&
           x.b.idx==y.b.idx && x.b.gen==y.b.gen;
}
static bool prox_contains(prox_pair_t* arr, int n, prox_pair_t p){
    for (int i=0;i<n;++i){
        if (arr[i].a.idx==p.a.idx && arr[i].a.gen==p.a.gen &&
            arr[i].b.idx==p.b.idx && arr[i].b.gen==p.b.gen) return true;
    }
    return false;
}

typedef struct { int i; } prox_iter_t;
typedef struct { ecs_entity_t a, b; } prox_view_t;

prox_iter_t prox_stay_begin(void){ return (prox_iter_t){ .i = -1 }; }
bool prox_stay_next(prox_iter_t* it, prox_view_t* out){
    int i = it->i + 1;
    while (i < prox_curr_count){
        if (ecs_alive_handle(prox_curr[i].a) && ecs_alive_handle(prox_curr[i].b)){
            it->i = i;
            *out = (prox_view_t){ prox_curr[i].a, prox_curr[i].b };
            return true;
        }
        ++i;
    }
    return false;
}

prox_iter_t prox_enter_begin(void){ return (prox_iter_t){ .i = -1 }; }
bool prox_enter_next(prox_iter_t* it, prox_view_t* out){
    for (int i = it->i + 1; i < prox_curr_count; ++i){
        if (!ecs_alive_handle(prox_curr[i].a) || !ecs_alive_handle(prox_curr[i].b)) continue;
        if (!prox_contains(prox_prev, prox_prev_count, prox_curr[i])){
            it->i = i; *out = (prox_view_t){ prox_curr[i].a, prox_curr[i].b }; return true;
        }
    }
    return false;
}

prox_iter_t prox_exit_begin(void){ return (prox_iter_t){ .i = -1 }; }
bool prox_exit_next(prox_iter_t* it, prox_view_t* out){
    for (int i = it->i + 1; i < prox_prev_count; ++i){
        if (!ecs_alive_handle(prox_prev[i].a) || !ecs_alive_handle(prox_prev[i].b)) continue;
        if (!prox_contains(prox_curr, prox_curr_count, prox_prev[i])){
            it->i = i; *out = (prox_view_t){ prox_prev[i].a, prox_prev[i].b }; return true;
        }
    }
    return false;
}

// =============== Helpers ==================
static inline int ent_index_checked(ecs_entity_t e) {
    return (e.idx < ECS_MAX_ENTITIES && ecs_gen[e.idx] == e.gen && e.gen != 0)
        ? (int)e.idx : -1;
}
static inline int ent_index_unchecked(ecs_entity_t e){ return (int)e.idx; }

static inline bool ecs_alive_idx(int i){ return ecs_gen[i] != 0; }
static inline bool ecs_alive_handle(ecs_entity_t e){ return ent_index_checked(e) >= 0; }
static inline ecs_entity_t handle_from_index(int i){ return (ecs_entity_t){ (uint32_t)i, ecs_gen[i] }; }

static ecs_entity_t find_player_handle(void){
    for(int i=0;i<ECS_MAX_ENTITIES;++i){
        if(ecs_alive_idx(i) && (ecs_mask[i]&CMP_PLAYER)) {
            return (ecs_entity_t){ (uint32_t)i, ecs_gen[i] };
        }
    }
    return ecs_null();
}

static float clampf(float v, float a, float b){ return (v<a)?a:((v>b)?b:v); }

static bool col_overlap_padded(int a, int b, float pad){
    float ax = cmp_pos[a].x, ay = cmp_pos[a].y;
    float bx = cmp_pos[b].x, by = cmp_pos[b].y;

    float ahx = (ecs_mask[a] & CMP_COL) ? (cmp_col[a].hx + pad) : pad;
    float ahy = (ecs_mask[a] & CMP_COL) ? (cmp_col[a].hy + pad) : pad;
    float bhx = (ecs_mask[b] & CMP_COL) ?  cmp_col[b].hx : 0.f;
    float bhy = (ecs_mask[b] & CMP_COL) ?  cmp_col[b].hy : 0.f;

    return fabsf(ax - bx) <= (ahx + bhx) && fabsf(ay - by) <= (ahy + bhy);
}

static void cmp_sprite_release_idx(int i){
    if (!(ecs_mask[i] & CMP_SPR)) return;
    if (asset_texture_valid(cmp_spr[i].tex)){
        asset_release_texture(cmp_spr[i].tex);
        cmp_spr[i].tex = (tex_handle_t){0,0};
    }
}

// =============== Public: lifecycle ========
static void ecs_register_builtin_systems(void);

void ecs_init(void){
    memset(ecs_mask, 0, sizeof(ecs_mask));
    memset(ecs_gen,  0, sizeof(ecs_gen));
    memset(ecs_next_gen, 0, sizeof(ecs_next_gen));
    free_top = 0;
    for (int i = ECS_MAX_ENTITIES - 1; i >= 0; --i) {
        free_stack[free_top++] = i;
    }

    ui_toast_timer = 0.0f;
    ui_toast_text[0] = '\0';

    prox_curr_count = prox_prev_count = 0;

    ecs_systems_init();
    ecs_register_builtin_systems();
}
void ecs_shutdown(void){ /* no heap to free currently */ }
void ecs_set_world_size(int w, int h){ g_worldW = w; g_worldH = h; }

// =============== Public: entity ===========
ecs_entity_t ecs_create(void)
{
    if (free_top == 0) return ecs_null();
    int idx = free_stack[--free_top];
    uint32_t g = ecs_next_gen[idx];
    if (g == 0) g = 1;
    ecs_gen[idx] = g;
    ecs_mask[idx] = 0;
    return (ecs_entity_t){ (uint32_t)idx, g };
}

void ecs_destroy(ecs_entity_t e)
{
    int idx = ent_index_checked(e);
    if (idx < 0) return;
    cmp_sprite_release_idx(idx);
    uint32_t g = ecs_gen[idx];
    g = (g + 1) ? (g + 1) : 1;
    ecs_gen[idx] = 0;
    ecs_next_gen[idx] = g;
    ecs_mask[idx] = 0;
    free_stack[free_top++] = idx;
}

// =============== Public: adders ===========
void cmp_add_position(ecs_entity_t e, float x, float y)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    cmp_pos[i] = (cmp_position_t){ x, y };
    ecs_mask[i] |= CMP_POS;
}

void cmp_add_velocity(ecs_entity_t e, float x, float y)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    cmp_vel[i] = (cmp_velocity_t){ x, y };
    ecs_mask[i] |= CMP_VEL;
}

void cmp_add_sprite_handle(ecs_entity_t e, tex_handle_t h, rectf src, float ox, float oy)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    if (asset_texture_valid(h)) asset_addref_texture(h);
    cmp_spr[i] = (cmp_sprite_t){ h, src, ox, oy };
    ecs_mask[i] |= CMP_SPR;
}

void cmp_add_sprite_path(ecs_entity_t e, const char* path, rectf src, float ox, float oy)
{
    tex_handle_t h = asset_acquire_texture(path);
    cmp_add_sprite_handle(e, h, src, ox, oy);
    asset_release_texture(h);
}

void cmp_add_player(ecs_entity_t e)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    ecs_mask[i] |= CMP_PLAYER;
}

void cmp_add_trigger(ecs_entity_t e, float pad, uint32_t target_mask){
    int i = ent_index_checked(e); if (i < 0) return;
    cmp_trigger[i] = (cmp_trigger_t){ pad, target_mask };
    ecs_mask[i] |= CMP_TRIGGER;
}

void cmp_add_billboard(ecs_entity_t e, const char* text, float y_off, float linger, billboard_state_t state){
    int i = ent_index_checked(e); if (i<0) return;
    strncpy(cmp_billboard[i].text, text, sizeof(cmp_billboard[i].text)-1);
    cmp_billboard[i].text[sizeof(cmp_billboard[i].text)-1] = '\0';
    cmp_billboard[i].y_offset = y_off;
    cmp_billboard[i].linger = linger;
    cmp_billboard[i].timer = 0;
    cmp_billboard[i].state = state;
    ecs_mask[i] |= CMP_BILLBOARD;
}

void cmp_add_inventory(ecs_entity_t e)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    cmp_inv[i] = (cmp_inventory_t){ 0, false };
    ecs_mask[i] |= CMP_INV;
}

void cmp_add_item(ecs_entity_t e, item_kind_t k)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    cmp_item[i] = (cmp_item_t){ k };
    ecs_mask[i] |= CMP_ITEM;
}

void cmp_add_vendor(ecs_entity_t e, item_kind_t sells, int price)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    cmp_vendor[i] = (cmp_vendor_t){ sells, price };
    ecs_mask[i] |= CMP_VENDOR;
}

void cmp_add_size(ecs_entity_t e, float hx, float hy)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    cmp_col[i] = (cmp_collider_t){ hx, hy };
    ecs_mask[i] |= CMP_COL;
}

// =============== Systems (internal) ======
static void sys_input(float dt, const input_t* in)
{
    (void)dt;
    const float SPEED = 200.0f;
    for (int e=0;e<ECS_MAX_ENTITIES;++e){
        if(!ecs_alive_idx(e)) continue;
        if ((ecs_mask[e]&(CMP_PLAYER|CMP_VEL))==(CMP_PLAYER|CMP_VEL)){
            cmp_vel[e].x = in->moveX * SPEED;
            cmp_vel[e].y = in->moveY * SPEED;
            break;
        }
    }
}

static void sys_physics(float dt)
{
    for (int e=0;e<ECS_MAX_ENTITIES;++e){
        if(!ecs_alive_idx(e)) continue;
        if ((ecs_mask[e]&(CMP_POS|CMP_VEL))==(CMP_POS|CMP_VEL)){
            cmp_pos[e].x += cmp_vel[e].x * dt;
            cmp_pos[e].y += cmp_vel[e].y * dt;
        }
    }
}

static void sys_bounds(void)
{
    int w=g_worldW, h=g_worldH;
    for (int e=0;e<ECS_MAX_ENTITIES;++e){
        if(!ecs_alive_idx(e)) continue;
        if (ecs_mask[e] & CMP_POS){
            if (ecs_mask[e] & CMP_COL){
                float hx = cmp_col[e].hx, hy = cmp_col[e].hy;
                cmp_pos[e].x = clampf(cmp_pos[e].x, hx, w - hx);
                cmp_pos[e].y = clampf(cmp_pos[e].y, hy, h - hy);
            } else {
                if(cmp_pos[e].x<0) cmp_pos[e].x=0;
                if(cmp_pos[e].y<0) cmp_pos[e].y=0;
                if(cmp_pos[e].x>w) cmp_pos[e].x=w;
                if(cmp_pos[e].y>h) cmp_pos[e].y=h;
            }
        }
    }
}

static void sys_proximity_build_view(void)
{
    if (prox_curr_count > 0) {
        memcpy(prox_prev, prox_curr, sizeof(prox_pair_t)*prox_curr_count);
    }
    prox_prev_count = prox_curr_count;
    prox_curr_count = 0;

    for (int a=0; a<ECS_MAX_ENTITIES; ++a){
        if(!ecs_alive_idx(a)) continue;
        if ((ecs_mask[a] & (CMP_POS|CMP_COL|CMP_TRIGGER)) != (CMP_POS|CMP_COL|CMP_TRIGGER)) continue;

        const cmp_trigger_t* tr = &cmp_trigger[a];

        for (int b=0; b<ECS_MAX_ENTITIES; ++b){
            if (b==a || !ecs_alive_idx(b)) continue;
            if ((ecs_mask[b] & tr->target_mask) != tr->target_mask) continue;
            if ((ecs_mask[b] & (CMP_POS|CMP_COL)) != (CMP_POS|CMP_COL)) continue;

            if (col_overlap_padded(a, b, tr->pad)){
                if (prox_curr_count < PROX_MAX){
                    prox_curr[prox_curr_count++] =
                        (prox_pair_t){ handle_from_index(a), handle_from_index(b) };
                }
            }
        }
    }
}

static void sys_billboards(float dt)
{
    for (int i=0;i<ECS_MAX_ENTITIES;++i){
        if (!ecs_alive_idx(i) || !(ecs_mask[i]&CMP_BILLBOARD)) continue;
        if (cmp_billboard[i].timer > 0) {
            cmp_billboard[i].timer -= dt;
            if (cmp_billboard[i].timer < 0) cmp_billboard[i].timer = 0;
        }
    }
    prox_iter_t it = prox_stay_begin(); prox_view_t v;
    while (prox_stay_next(&it, &v)){
        int a = ent_index_checked(v.a);
        if (a >= 0 && (ecs_mask[a] & CMP_BILLBOARD)){
            cmp_billboard[a].timer = cmp_billboard[a].linger;
        }
    }
}

static void sys_pickups_from_proximity(void)
{
    prox_iter_t it = prox_enter_begin(); prox_view_t v;
    while (prox_enter_next(&it, &v)){
        int ia = ent_index_checked(v.a);
        int ib = ent_index_checked(v.b);
        if (ia<0 || ib<0) continue;
        if ((ecs_mask[ia]&CMP_INV) && (ecs_mask[ib]&CMP_ITEM) && cmp_item[ib].kind==ITEM_COIN){
            cmp_inv[ia].coins += 1;
            ecs_destroy(v.b);
            ui_toast(1.0f, "Picked up a coin! (%d)", cmp_inv[ia].coins);
        }
    }
}

static void try_buy_hat(ecs_entity_t player, ecs_entity_t vendor)
{
    int ia = ent_index_checked(player);
    int ib = ent_index_checked(vendor);
    if((ecs_mask[ib]&CMP_VENDOR)==0) return;
    if((ecs_mask[ia]&CMP_INV)==0) return;
    {
        cmp_vendor_t v = cmp_vendor[ib];
        cmp_inventory_t *pInv = &cmp_inv[ia];
        if(v.sells==ITEM_HAT){
            if(pInv->hasHat){
                ui_toast(1.5f, "You already have a hat.");
                return;
            }
            if(pInv->coins >= v.price){
                pInv->coins -= v.price;
                pInv->hasHat = true;

                if ((ecs_mask[ia] & CMP_SPR)){
                    if (asset_texture_valid(cmp_spr[ia].tex)) asset_release_texture(cmp_spr[ia].tex);
                    tex_handle_t hatTexture = asset_acquire_texture("assets/player_hat.png");
                    cmp_spr[ia].tex = hatTexture;

                    int w=0, h=0;
                    asset_texture_size(hatTexture, &w, &h);
                    cmp_spr[ia].src = rectf_xywh(0,0,(float)w,(float)h);
                    cmp_spr[ia].ox  = w * 0.5f;
                    cmp_spr[ia].oy  = h * 0.5f;
                }
                ui_toast(1.5f, "Bought hat for %d coins!", v.price);
            } else {
                ui_toast(1.5f, "Need %d coins for hat (you have %d).", v.price, pInv->coins);
            }
        }
    }
}

static void sys_interact_from_proximity(const input_t* in)
{
    if (!input_pressed(in, BTN_INTERACT)) return;
    ecs_entity_t player_h = find_player_handle();
    int p = ent_index_checked(player_h);
    if (p < 0 || !(ecs_mask[p] & (CMP_COL | CMP_POS))) return;

    float best_d2 = INFINITY;
    int   best_vendor = -1;
    prox_iter_t it = prox_stay_begin();
    prox_view_t v;
    while (prox_stay_next(&it, &v)) {
        int ia = ent_index_checked(v.a);
        int ib = ent_index_checked(v.b);
        if (ia < 0 || ib < 0) continue;

        int vendor = -1;
        int player = -1;

        if ((ecs_mask[ia] & CMP_VENDOR) && ib == p) {
            vendor = ia; player = ib;
        } else if ((ecs_mask[ib] & CMP_VENDOR) && ia == p) {
            vendor = ib; player = ia;
        } else {
            continue;
        }

        if ((ecs_mask[vendor] & CMP_POS) == 0 || (ecs_mask[player] & CMP_POS) == 0) continue;

        float dx = cmp_pos[vendor].x - cmp_pos[player].x;
        float dy = cmp_pos[vendor].y - cmp_pos[player].y;
        float d2 = dx*dx + dy*dy;

        if (d2 < best_d2) {
            best_d2 = d2;
            best_vendor = vendor;
        }
    }

    if (best_vendor >= 0) {
        try_buy_hat(player_h, handle_from_index(best_vendor));
    }
}

void sys_debug_binds(const input_t* in)
{
    if(input_pressed(in, BTN_ASSET_DEBUG_PRINT)) {
        asset_reload_all();
        asset_log_debug();
  }
}

// ========= Public: update/iterators ============
void ecs_tick(float dt, const input_t* in)
{
    if(ui_toast_timer > 0) ui_toast_timer -= dt;

    ecs_run_phase(PHASE_INPUT,       dt, in);
    ecs_run_phase(PHASE_PHYSICS,     dt, in);
    ecs_run_phase(PHASE_SIM_PRE,     dt, in);
    ecs_run_phase(PHASE_SIM_POST,    dt, in);
    ecs_run_phase(PHASE_DEBUG,       dt, in);
}

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

        float collider_hx = 0;
        float collider_hy = 0;
        if(ecs_mask[i] & CMP_COL) {

            collider_hx = cmp_col[i].hx;
            collider_hy = cmp_col[i].hy;
        }

        *out = (ecs_trigger_view_t){
            .x = cmp_pos[i].x, .y = cmp_pos[i].y,
            .hx = collider_hx, .hy = collider_hy,
            .pad = cmp_trigger[i].pad,
        };
        return true;
    }
    return false;
}

// --- BILLBOARDS (iterator for renderer) ---
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
            .x = cmp_pos[i].x,
            .y = cmp_pos[i].y,
            .y_offset = cmp_billboard[i].y_offset,
            .alpha = a,
            .text = cmp_billboard[i].text
        };
        return true;
    }
    return false;
}

// --- UI / HINTS (data only) ---
bool        ecs_toast_is_active(void) { return ui_toast_timer > 0.0f; }
const char* ecs_toast_get_text(void)   { return ui_toast_text; }

bool ecs_vendor_hint_is_active(int* out_x, int* out_y, const char** out_text)
{
    ecs_entity_t h = find_player_handle();
    int p = ent_index_checked(h);
    if (p < 0 || !(ecs_mask[p] & CMP_COL)) return false;

    for (int v = 0; v < ECS_MAX_ENTITIES; ++v) {
        if (!ecs_alive_idx(v)) continue;
        if ((ecs_mask[v] & (CMP_VENDOR | CMP_COL)) != (CMP_VENDOR | CMP_COL)) continue;
        if (v == p) continue;

        if (col_overlap_padded(p, v, 30.0f)) {
            static char buf[64];
            snprintf(buf, sizeof(buf), "Press E to buy hat (%d)", cmp_vendor[v].price);
            if (out_text) *out_text = buf;
            if (out_x)    *out_x = (int)cmp_pos[v].x;
            if (out_y)    *out_y = (int)cmp_pos[v].y;
            return true;
        }
    }
    return false;
}

ecs_count_result_t ecs_count_entities(const uint32_t* masks, int num_masks)
{
    // WARNING: this might blow up if more than 100 masks as ecs_count_result_t type is max size 100 in array
    ecs_count_result_t result = { .num = num_masks };
    memset(result.count, 0, sizeof(result.count));

    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        uint32_t mask = ecs_mask[i];
        for (int j = 0; j < num_masks; ++j) {
            if ((mask & masks[j]) == masks[j]) {
                result.count[j]++;
            }
        }
    }
    return result;
}

void ecs_get_player_stats(int* outCoins, bool* outHasHat)
{
    ecs_entity_t handle = find_player_handle();
    int p = ent_index_checked(handle);

    int coins = 0; bool hat=false;
    if (p >= 0 && (ecs_mask[p]&CMP_INV)) { coins = cmp_inv[p].coins; hat = cmp_inv[p].hasHat; }
    if (outCoins) *outCoins = coins;
    if (outHasHat) *outHasHat = hat;
}

// =============== Adapters for system registry =========
static void sys_input_adapt(float dt, const input_t* in) { sys_input(dt, in); }
static void sys_physics_adapt(float dt, const input_t* in) { (void)in; sys_physics(dt); }
static void sys_bounds_adapt(float dt, const input_t* in) { (void)dt; (void)in; sys_bounds(); }
static void sys_prox_build_adapt(float dt, const input_t* in) { (void)dt; (void)in; sys_proximity_build_view(); }
static void sys_billboards_adapt(float dt, const input_t* in) { (void)in; sys_billboards(dt); }
static void sys_pickups_adapt(float dt, const input_t* in) { (void)dt; (void)in; sys_pickups_from_proximity(); }
static void sys_interact_adapt(float dt, const input_t* in) { (void)dt; sys_interact_from_proximity(in); }
static void sys_debug_binds_adapt(float dt, const input_t* in) { (void)dt; sys_debug_binds(in); }

// =============== Registration =========
static void ecs_register_builtin_systems(void)
{
    ecs_register_system(PHASE_INPUT,    0, sys_input_adapt, "input");

    ecs_register_system(PHASE_PHYSICS,  100, sys_physics_adapt, "physics");
    ecs_register_system(PHASE_PHYSICS,  200, sys_bounds_adapt, "bounds");

    ecs_register_system(PHASE_SIM_PRE,  100, sys_prox_build_adapt, "proximity_view");

    ecs_register_system(PHASE_SIM_POST, 100, sys_pickups_adapt, "pickups");
    ecs_register_system(PHASE_SIM_POST, 200, sys_billboards_adapt, "billboards");
    ecs_register_system(PHASE_SIM_POST, 300, sys_interact_adapt, "interact");

    ecs_register_system(PHASE_DEBUG,    100, sys_debug_binds_adapt, "debug_binds");
}
