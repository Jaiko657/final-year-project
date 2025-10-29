#include "ecs.h"
#include "logger.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/*
 * TODO:
 * - remove Raylib types
 * - Add Generations (PRIORITY)
 * - remove event stuff, need to do proper pipelines
 * - registration of systems etc (modularity)
 * - duplication of size in sprite and collider?
*/

// ================= Debug =================
#ifndef DEBUG_COLLISION
#define DEBUG_COLLISION 1
#endif

// =============== Bitmasks / Tags =========
#define CMP_POS       (1<<0)
#define CMP_VEL       (1<<1)
#define CMP_SPR       (1<<2)
#define TAG_PLAYER    (1<<4)
#define CMP_ITEM      (1<<5)
#define CMP_INV       (1<<6)
#define CMP_VENDOR    (1<<7)
#define CMP_COL       (1<<8)

// =============== Components (internal) ===
typedef struct { float x, y; } cmp_position_t;
typedef struct { float x, y; } cmp_velocity_t;
typedef struct { Texture2D tex; Rectangle src; float ox, oy; } cmp_sprite_t;
typedef struct { float hx, hy; } cmp_collider_t;
typedef struct { item_kind_t kind; } cmp_item_t;
typedef struct { int coins; bool hasHat; } cmp_inventory_t;
typedef struct { item_kind_t sells; int price; } cmp_vendor_t;

// =============== ECS Storage =============
static uint32_t        ecs_mask[ECS_MAX_ENTITIES];
static uint32_t        ecs_gen[ECS_MAX_ENTITIES];       // 0 is dead; >0 is live generation
static uint32_t        ecs_next_gen[ECS_MAX_ENTITIES];  // next generation to use when resurrecting
static cmp_position_t  cmp_pos[ECS_MAX_ENTITIES];
static cmp_velocity_t  cmp_vel[ECS_MAX_ENTITIES];
static cmp_sprite_t    cmp_spr[ECS_MAX_ENTITIES];
static cmp_collider_t  cmp_col[ECS_MAX_ENTITIES];
static cmp_item_t      cmp_item[ECS_MAX_ENTITIES];
static cmp_inventory_t cmp_inv[ECS_MAX_ENTITIES];
static cmp_vendor_t    cmp_vendor[ECS_MAX_ENTITIES];

// ========== O(1) create/delete ==========
static int      free_stack[ECS_MAX_ENTITIES];
static int      free_top = 0;

// Config
static int g_worldW = 800, g_worldH = 450;
static Texture2D g_hatTexture = (Texture2D){0};

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

// =============== Events ===================
typedef enum { EV_PICKUP, EV_TRY_BUY } ev_type_t;
typedef struct { ev_type_t type; ecs_entity_t a, b; } ev_t;

#define EV_MAX 128
static ev_t ev_queue[EV_MAX];
static int  ev_count = 0;

static void ev_emit(ev_t e){
    if (ev_count < EV_MAX) {
        ev_queue[ev_count++] = e;
        LOGC(LOGCAT_ECS, LOG_LVL_INFO, "Emit event:\t type=%d a=(%u,%u) b=(%u,%u)",
                 e.type, e.a.idx, e.a.gen, e.b.idx, e.b.gen);
    }
}

// =============== Helpers ==================
static inline int ent_index_checked(ecs_entity_t e) {
    return (e.idx < ECS_MAX_ENTITIES && ecs_gen[e.idx] == e.gen && e.gen != 0)
        ? (int)e.idx : -1;
}
// Fast unchecked index when you already validated
static inline int ent_index_unchecked(ecs_entity_t e){ return (int)e.idx; }

static inline bool ecs_alive_idx(int i){ return ecs_gen[i] != 0; }
static inline bool ecs_alive_handle(ecs_entity_t e){ return ent_index_checked(e) >= 0; }
static inline ecs_entity_t handle_from_index(int i){ return (ecs_entity_t){ (uint32_t)i, ecs_gen[i] }; }

static ecs_entity_t find_player_handle(void){
    for(int i=0;i<ECS_MAX_ENTITIES;++i){
        if(ecs_alive_idx(i) && (ecs_mask[i]&TAG_PLAYER)) {
            return (ecs_entity_t){ (uint32_t)i, ecs_gen[i] };
        }
    }
    return ecs_null();
}

static float clampf(float v, float a, float b){ return (v<a)?a:((v>b)?b:v); }

static bool col_overlap_padded(int a, int b, float pad){
    float ax=cmp_pos[a].x, ay=cmp_pos[a].y;
    float bx=cmp_pos[b].x, by=cmp_pos[b].y;

    float ahx = (ecs_mask[a]&CMP_COL)? (cmp_col[a].hx+pad) : pad;
    float ahy = (ecs_mask[a]&CMP_COL)? (cmp_col[a].hy+pad) : pad;
    float bhx = (ecs_mask[b]&CMP_COL)? (cmp_col[b].hx+pad) : pad;
    float bhy = (ecs_mask[b]&CMP_COL)? (cmp_col[b].hy+pad) : pad;

    return fabsf(ax-bx) <= (ahx+bhx) && fabsf(ay-by) <= (ahy+bhy);
}

// =============== Public: lifecycle ========
void ecs_init(void){
    memset(ecs_mask, 0, sizeof(ecs_mask));
    memset(ecs_gen,  0, sizeof(ecs_gen));
    memset(ecs_next_gen, 0, sizeof(ecs_next_gen));
    // Build free-list: all slots free
    free_top = 0;
    for (int i = ECS_MAX_ENTITIES - 1; i >= 0; --i) { // push in reverse for cachey low indices first
        free_stack[free_top++] = i;
    }

    ev_count = 0;
    ui_toast_timer = 0.0f;
    ui_toast_text[0] = '\0';
}
void ecs_shutdown(void){ /* no heap to free currently */ }
void ecs_set_world_size(int w, int h){ g_worldW = w; g_worldH = h; }
void ecs_set_hat_texture(Texture2D tex){ g_hatTexture = tex; }

// =============== Public: entity ===========
ecs_entity_t ecs_create(void)
{
    if (free_top == 0) return ecs_null();
    int idx = free_stack[--free_top];
    uint32_t g = ecs_next_gen[idx];
    if (g == 0) g = 1;                    // first time ever
    ecs_gen[idx] = g;                     // alive with current generation
    ecs_mask[idx] = 0;
    return (ecs_entity_t){ (uint32_t)idx, g };
}

void ecs_destroy(ecs_entity_t e)
{
    int idx = ent_index_checked(e);
    if (idx < 0) return;
    uint32_t g = ecs_gen[idx];
    g = (g + 1) ? (g + 1) : 1;            // bump; avoid 0
    ecs_gen[idx] = 0;                     // dead
    ecs_next_gen[idx] = g;                // remember next live generation
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

void cmp_add_sprite(ecs_entity_t e, Texture2D t, Rectangle src, float ox, float oy)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    cmp_spr[i] = (cmp_sprite_t){ t, src, ox, oy };
    ecs_mask[i] |= CMP_SPR;
}

void tag_add_player(ecs_entity_t e)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    ecs_mask[i] |= TAG_PLAYER;
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
        if ((ecs_mask[e]&(TAG_PLAYER|CMP_VEL))==(TAG_PLAYER|CMP_VEL)){
            // uses your input_t exactly
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

static void sys_proximity(const input_t* in)
{
    ecs_entity_t player_handle = find_player_handle();
    int player = ent_index_checked(player_handle);
    if(player<0) return;

    const float PICKUP_PAD = 2.0f;
    for (int it=0; it<ECS_MAX_ENTITIES; ++it){
        if(!ecs_alive_idx(it)) continue;
        if((ecs_mask[it] & (CMP_ITEM|CMP_COL)) != (CMP_ITEM|CMP_COL)) continue;
        if(cmp_item[it].kind != ITEM_COIN) continue;

        if ((ecs_mask[player] & CMP_COL) && col_overlap_padded(player, it, PICKUP_PAD)){
            ev_emit((ev_t){EV_PICKUP, player_handle, handle_from_index(it)});
        }
    }

    if (input_pressed(in, BTN_INTERACT)){
        const float INTERACT_PAD = 30.0f;
        for(int v=0; v<ECS_MAX_ENTITIES; ++v){
            if(!ecs_alive_idx(v)) continue;
            if(!(ecs_mask[v]&CMP_VENDOR)) continue;
            if(!(ecs_mask[v]&CMP_COL) || !(ecs_mask[player]&CMP_COL)) continue;
            if (col_overlap_padded(player, v, INTERACT_PAD)){
                ev_emit((ev_t){EV_TRY_BUY, player_handle, handle_from_index(v)});
                break;
            }
        }
    }
}

static void sys_events(void)
{
    for(int i=0;i<ev_count;++i){
        ev_t ev = ev_queue[i];
        int ia = ent_index_checked(ev.a);
        int ib = ent_index_checked(ev.b);
        if (ia < 0 || ib < 0) continue; // stale, ignore TODO: Does this leak
        LOGC(LOGCAT_ECS, LOG_LVL_INFO, "Handle event:\t type=%d a=(%u,%u) b=(%u,%u)",
                 ev.type, ev.a.idx, ev.a.gen, ev.b.idx, ev.b.gen);
        switch(ev.type){
            case EV_PICKUP:
                if((ecs_mask[ia]&CMP_INV) && (ecs_mask[ib]&CMP_ITEM) && cmp_item[ib].kind==ITEM_COIN){
                    cmp_inv[ia].coins += 1;
                    ecs_destroy(ev.b);
                    ui_toast(1.0f, "Picked up a coin! (%d)", cmp_inv[ia].coins);
                }
                break;

            case EV_TRY_BUY:
                if((ecs_mask[ib]&CMP_VENDOR)==0) break;
                if((ecs_mask[ia]&CMP_INV)==0) break;
                {
                    cmp_vendor_t v = cmp_vendor[ib];
                    cmp_inventory_t *pInv = &cmp_inv[ia];

                    if(v.sells==ITEM_HAT){
                        if(pInv->hasHat){
                            ui_toast(1.5f, "You already have a hat.");
                            break;
                        }
                        if(pInv->coins >= v.price){
                            pInv->coins -= v.price;
                            pInv->hasHat = true;

                            if ((ecs_mask[ia]&CMP_SPR) && g_hatTexture.id != 0){
                                cmp_spr[ia].tex = g_hatTexture;
                                cmp_spr[ia].src = (Rectangle){0,0,(float)g_hatTexture.width,(float)g_hatTexture.height};
                                cmp_spr[ia].ox  = g_hatTexture.width/2.0f;
                                cmp_spr[ia].oy  = g_hatTexture.height/2.0f;
                            }
                            ui_toast(1.5f, "Bought hat for %d coins!", v.price);
                        } else {
                            ui_toast(1.5f, "Need %d coins for hat (you have %d).", v.price, pInv->coins);
                        }
                    }
                }
                break;
        }
    }
    ev_count = 0;
}

// =============== Public: update/render ===
void ecs_tick(float fixed_dt, const input_t* in)
{
    if(ui_toast_timer > 0.0f) ui_toast_timer -= fixed_dt;
    sys_input(fixed_dt, in);
    sys_physics(fixed_dt);
    sys_bounds();
    sys_proximity(in);
    sys_events();
}

void ecs_render_world(void)
{
    for (int e=0;e<ECS_MAX_ENTITIES;++e){
        if(!ecs_alive_idx(e)) continue;
        if ((ecs_mask[e]&(CMP_POS|CMP_SPR))==(CMP_POS|CMP_SPR)){
            DrawTexturePro(
                cmp_spr[e].tex,
                cmp_spr[e].src,
                (Rectangle){cmp_pos[e].x, cmp_pos[e].y, cmp_spr[e].src.width, cmp_spr[e].src.height},
                (Vector2){cmp_spr[e].ox, cmp_spr[e].oy},
                0.0f,
                WHITE
            );
        }
    }

    // Toast at top
    if(ui_toast_timer > 0.0f){
        int W = GetScreenWidth();
        const int fontSize = 20;
        int tw = MeasureText(ui_toast_text, fontSize);
        int x = (W - tw)/2, y = 10;
        DrawRectangle(x-8, y-4, tw+16, 28, (Color){0,0,0,180});
        DrawText(ui_toast_text, x, y, fontSize, RAYWHITE);
    }
}

void ecs_draw_vendor_hints(void)
{
    ecs_entity_t handle = find_player_handle();
    int p = ent_index_checked(handle);

    if (p < 0 || !(ecs_mask[p] & CMP_COL)) return;

    for (int v=0; v<ECS_MAX_ENTITIES; ++v){
        if(!ecs_alive_idx(v)) continue;
        if ((ecs_mask[v] & (CMP_VENDOR | CMP_COL)) != (CMP_VENDOR | CMP_COL)) continue;
        if (v == p) continue;

        if (col_overlap_padded(p, v, 30.0f)){
            char msg[64];
            snprintf(msg, sizeof(msg), "Press E to buy hat (%d)", cmp_vendor[v].price);

            int tw = MeasureText(msg, 18);
            int x = (int)(cmp_pos[v].x - tw/2), y = (int)(cmp_pos[v].y - 32);

            DrawRectangle(x-6, y-6, tw+12, 26, (Color){0,0,0,160});
            DrawText(msg, x, y, 18, RAYWHITE);
            break;
        }
    }
}

void ecs_debug_draw(void)
{
#if DEBUG_COLLISION
    for (int e=0; e<ECS_MAX_ENTITIES; ++e){
        if(!ecs_alive_idx(e)) continue;
        if((ecs_mask[e] & (CMP_POS | CMP_COL)) == (CMP_POS | CMP_COL)){
            float x = cmp_pos[e].x, y = cmp_pos[e].y;
            float hx = cmp_col[e].hx, hy = cmp_col[e].hy;

            int rx = (int)floorf(x - hx);
            int ry = (int)floorf(y - hy);
            int rw = (int)ceilf(hx * 2.0f);
            int rh = (int)ceilf(hy * 2.0f);

            DrawRectangleLines(rx, ry, rw, rh, RED);
        }
    }

    int fps = GetFPS();
    float frameTime = GetFrameTime() * 1000.0f;
    char buf[64];
    snprintf(buf, sizeof(buf), "FPS: %d | Frame: %.2f ms", fps, frameTime);

    int fontSize = 18;
    int tw = MeasureText(buf, fontSize);
    int x = (GetScreenWidth() - tw) / 2;
    int y = GetScreenHeight() - fontSize - 6;

    DrawRectangle(x - 8, y - 4, tw + 16, fontSize + 8, (Color){0,0,0,160});
    DrawText(buf, x, y, fontSize, RAYWHITE);
#endif
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
