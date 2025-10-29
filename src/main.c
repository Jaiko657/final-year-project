#include "raylib.h"
#include "components/input.h"
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

// ========================= Debug =========================
#ifndef DEBUG_COLLISION
#define DEBUG_COLLISION 1   // set to 0 to disable collider outlines
#endif

// ========================= Config / Tags =========================
#define ECS_MAX_ENTITIES 1024

#define CMP_POS       (1<<0)
#define CMP_VEL       (1<<1)
#define CMP_SPR       (1<<2)
#define TAG_PLAYER    (1<<4)
#define CMP_ITEM      (1<<5)
#define CMP_INV       (1<<6)
#define CMP_VENDOR    (1<<7)
#define CMP_COL       (1<<8)

// ========================= Types =========================
typedef int32_t ecs_entity_t;

typedef struct { float x, y; } cmp_position_t;
typedef struct { float x, y; } cmp_velocity_t;
typedef struct { Texture2D tex; Rectangle src; float ox, oy; } cmp_sprite_t;

// *** simplified collider: every entity is an AABB with half-extents ***
typedef struct { float hx, hy; } cmp_collider_t;

typedef enum { ITEM_COIN=1, ITEM_HAT=2 } item_kind_t;
typedef struct { item_kind_t kind; } cmp_item_t;

typedef struct { int coins; bool hasHat; } cmp_inventory_t;

typedef struct { item_kind_t sells; int price; } cmp_vendor_t;

// ========================= ECS Storage =========================
static uint32_t       ecs_mask[ECS_MAX_ENTITIES];
static bool           ecs_alive[ECS_MAX_ENTITIES];
static cmp_position_t  cmp_pos[ECS_MAX_ENTITIES];
static cmp_velocity_t  cmp_vel[ECS_MAX_ENTITIES];
static cmp_sprite_t    cmp_spr[ECS_MAX_ENTITIES];
static cmp_collider_t  cmp_col[ECS_MAX_ENTITIES];
static cmp_item_t      cmp_item[ECS_MAX_ENTITIES];
static cmp_inventory_t cmp_inv[ECS_MAX_ENTITIES];
static cmp_vendor_t    cmp_vendor[ECS_MAX_ENTITIES];

// ========================= Assets (global) =========================
static Texture2D tex_player, tex_player_hat, tex_coin, tex_npc;

// ========================= UI / Toast =========================
static char  ui_toast_text[128] = {0};
static float ui_toast_timer = 0.0f;

static void ui_toast(float secs, const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    vsnprintf(ui_toast_text, sizeof(ui_toast_text), fmt, ap);
    va_end(ap);
    ui_toast_timer = secs;
}

// ========================= Events =========================
typedef enum { EV_PICKUP, EV_TRY_BUY } ev_type_t;
typedef struct { ev_type_t type; int a, b; } ev_t;

#define EV_MAX 128
static ev_t ev_queue[EV_MAX];
static int  ev_count = 0;

static void ev_emit(ev_t e)
{
    if (ev_count < EV_MAX) {
        ev_queue[ev_count++] = e;
        TraceLog(LOG_INFO, "Emit event:\t type=%d source=%d target=%d", e.type, e.a, e.b);
    }
}

// ========================= ECS API =========================
static ecs_entity_t ecs_create(void)
{
    for (int i=0;i<ECS_MAX_ENTITIES;++i){
        if (!ecs_alive[i]) { ecs_alive[i]=true; ecs_mask[i]=0; return i; }
    }
    return -1;
}

static void ecs_destroy(ecs_entity_t e)
{
    if (e>=0 && e<ECS_MAX_ENTITIES) { ecs_alive[e]=false; ecs_mask[e]=0; }
}

// ========================= Component Adders =========================
static void cmp_add_position(ecs_entity_t e, float x, float y)
{ cmp_pos[e]=(cmp_position_t){x,y}; ecs_mask[e]|=CMP_POS; }

static void cmp_add_velocity(ecs_entity_t e, float x, float y)
{ cmp_vel[e]=(cmp_velocity_t){x,y}; ecs_mask[e]|=CMP_VEL; }

static void cmp_add_sprite(ecs_entity_t e, Texture2D t, Rectangle src, float ox, float oy)
{ cmp_spr[e]=(cmp_sprite_t){t,src,ox,oy}; ecs_mask[e]|=CMP_SPR; }

static void tag_add_player(ecs_entity_t e)
{ ecs_mask[e]|=TAG_PLAYER; }

static void cmp_add_inventory(ecs_entity_t e)
{ cmp_inv[e]=(cmp_inventory_t){0,false}; ecs_mask[e]|=CMP_INV; }

static void cmp_add_item(ecs_entity_t e, item_kind_t k)
{ cmp_item[e]=(cmp_item_t){k}; ecs_mask[e]|=CMP_ITEM; }

static void cmp_add_vendor(ecs_entity_t e, item_kind_t sells, int price)
{ cmp_vendor[e]=(cmp_vendor_t){sells,price}; ecs_mask[e]|=CMP_VENDOR; }

// *** single size component for all collisions (AABB) ***
static void cmp_add_size(ecs_entity_t e, float hx, float hy)
{ cmp_col[e]=(cmp_collider_t){hx,hy}; ecs_mask[e]|=CMP_COL; }

// ========================= Systems =========================

static void sys_input(float dt, const input_t* in)
{
    (void)dt;
    const float SPEED = 200.0f;
    for (int e=0;e<ECS_MAX_ENTITIES;++e){
        if(!ecs_alive[e]) continue;
        if ((ecs_mask[e]&(TAG_PLAYER|CMP_VEL))==(TAG_PLAYER|CMP_VEL)){
            cmp_vel[e].x = in->moveX * SPEED;
            cmp_vel[e].y = in->moveY * SPEED;
            break;
        }
    }
}

// *** simple AABB overlap with optional padding ***
static bool col_overlap_padded(int a, int b, float pad){
    float ax=cmp_pos[a].x, ay=cmp_pos[a].y;
    float bx=cmp_pos[b].x, by=cmp_pos[b].y;

    float ahx = (ecs_mask[a]&CMP_COL)? (cmp_col[a].hx+pad) : pad;
    float ahy = (ecs_mask[a]&CMP_COL)? (cmp_col[a].hy+pad) : pad;
    float bhx = (ecs_mask[b]&CMP_COL)? (cmp_col[b].hx+pad) : pad;
    float bhy = (ecs_mask[b]&CMP_COL)? (cmp_col[b].hy+pad) : pad;

    return fabsf(ax-bx) <= (ahx+bhx) && fabsf(ay-by) <= (ahy+bhy);
}

static void sys_physics(float dt)
{
    for (int e=0;e<ECS_MAX_ENTITIES;++e){
        if(!ecs_alive[e]) continue;
        if ((ecs_mask[e]&(CMP_POS|CMP_VEL))==(CMP_POS|CMP_VEL)){
            cmp_pos[e].x += cmp_vel[e].x * dt;
            cmp_pos[e].y += cmp_vel[e].y * dt;
        }
    }
}

static float clampf(float v, float a, float b){ return (v<a)?a:((v>b)?b:v); }

static void sys_bounds(float dt, int w, int h)
{
    (void)dt;
    for (int e=0;e<ECS_MAX_ENTITIES;++e){
        if(!ecs_alive[e]) continue;
        if (ecs_mask[e] & CMP_POS){
            if (ecs_mask[e] & CMP_COL){
                float hx = cmp_col[e].hx, hy = cmp_col[e].hy;
                cmp_pos[e].x = clampf(cmp_pos[e].x, hx, w - hx);
                cmp_pos[e].y = clampf(cmp_pos[e].y, hy, h - hy);
            } else {
                // fallback if no size
                if(cmp_pos[e].x<0) cmp_pos[e].x=0;
                if(cmp_pos[e].y<0) cmp_pos[e].y=0;
                if(cmp_pos[e].x>w) cmp_pos[e].x=w;
                if(cmp_pos[e].y>h) cmp_pos[e].y=h;
            }
        }
    }
}

static void sys_proximity(float dt, const input_t* in)
{
    (void)dt;
    int player=-1;
    for(int e=0;e<ECS_MAX_ENTITIES;++e){ if(ecs_alive[e] && (ecs_mask[e]&TAG_PLAYER)){ player=e; break; } }
    if(player<0) return;

    const float PICKUP_PAD = 2.0f;
    for (int it=0; it<ECS_MAX_ENTITIES; ++it){
        if(!ecs_alive[it]) continue;
        if((ecs_mask[it] & (CMP_ITEM|CMP_COL)) != (CMP_ITEM|CMP_COL)) continue;
        if(cmp_item[it].kind != ITEM_COIN) continue;

        if ((ecs_mask[player] & CMP_COL) &&
            col_overlap_padded(player, it, PICKUP_PAD)){
            ev_emit((ev_t){EV_PICKUP, player, it});
        }
    }

    // Interact with vendor (edge-triggered)
    if (input_pressed(in, BTN_INTERACT)){
        const float INTERACT_PAD = 30.0f;
        for(int v=0; v<ECS_MAX_ENTITIES; ++v){
            if(!ecs_alive[v]) continue;
            if(!(ecs_mask[v]&CMP_VENDOR)) continue;
            if(!(ecs_mask[v]&CMP_COL) || !(ecs_mask[player]&CMP_COL)) continue;
            if (col_overlap_padded(player, v, INTERACT_PAD)){
                ev_emit((ev_t){EV_TRY_BUY, player, v});
                break;
            }
        }
    }
}

static void sys_events(void)
{
    for(int i=0;i<ev_count;++i){
        ev_t e = ev_queue[i];
        TraceLog(LOG_INFO, "Handle event:\t type=%d source=%d target=%d", e.type, e.a, e.b);
        switch(e.type){
            case EV_PICKUP:
                if((ecs_mask[e.a]&CMP_INV) && (ecs_mask[e.b]&CMP_ITEM) && cmp_item[e.b].kind==ITEM_COIN){
                    cmp_inv[e.a].coins += 1;
                    ecs_destroy(e.b);
                    ui_toast(1.0f, "Picked up a coin! (%d)", cmp_inv[e.a].coins);
                }
                break;

            case EV_TRY_BUY:
                if((ecs_mask[e.b]&CMP_VENDOR)==0) break;
                if((ecs_mask[e.a]&CMP_INV)==0) break;
                {
                    cmp_vendor_t v = cmp_vendor[e.b];
                    cmp_inventory_t *pInv = &cmp_inv[e.a];

                    if(v.sells==ITEM_HAT){
                        if(pInv->hasHat){
                            ui_toast(1.5f, "You already have a hat.");
                            break;
                        }
                        if(pInv->coins >= v.price){
                            pInv->coins -= v.price;
                            pInv->hasHat = true;
                            // swap sprite to hatted version
                            if(ecs_mask[e.a]&CMP_SPR){
                                cmp_spr[e.a].tex = tex_player_hat;
                                cmp_spr[e.a].src = (Rectangle){0,0,(float)tex_player_hat.width,(float)tex_player_hat.height};
                                cmp_spr[e.a].ox  = tex_player_hat.width/2.0f;
                                cmp_spr[e.a].oy  = tex_player_hat.height/2.0f;
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

static void sys_render(void)
{
    for (int e=0;e<ECS_MAX_ENTITIES;++e){
        if(!ecs_alive[e]) continue;
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
}

static void debug_draw_colliders(void)
{
#if DEBUG_COLLISION
    for (int e=0; e<ECS_MAX_ENTITIES; ++e){
        if(!ecs_alive[e]) continue;
        if((ecs_mask[e] & (CMP_POS | CMP_COL)) == (CMP_POS | CMP_COL)){
            float x = cmp_pos[e].x, y = cmp_pos[e].y;
            float hx = cmp_col[e].hx, hy = cmp_col[e].hy;

            // AABB is centered at (x,y) with half-extents (hx,hy)
            int rx = (int)floorf(x - hx);
            int ry = (int)floorf(y - hy);
            int rw = (int)ceilf(hx * 2.0f);
            int rh = (int)ceilf(hy * 2.0f);

            DrawRectangleLines(rx, ry, rw, rh, RED); // 1px outline
        }
    }
    // ==================== DEBUG: FPS & Frame Time ====================
    int fps = GetFPS();
    float frameTime = GetFrameTime() * 1000.0f; // ms
    char buf[64];
    snprintf(buf, sizeof(buf), "FPS: %d | Frame: %.2f ms", fps, frameTime);

    int fontSize = 18;
    int tw = MeasureText(buf, fontSize);
    int x = (GetScreenWidth() - tw) / 2;
    int y = GetScreenHeight() - fontSize - 6;

    DrawRectangle(x - 8, y - 4, tw + 16, fontSize + 8, (Color){0,0,0,160});
    DrawText(buf, x, y, fontSize, RAYWHITE);
    // ================================================================
#endif
}

static int find_player(void){
    for(int e=0;e<ECS_MAX_ENTITIES;++e)
        if(ecs_alive[e] && (ecs_mask[e]&TAG_PLAYER)) return e;
    return -1;
}

// ========================= Main =========================
#define TRY_TEX(var, path) do{ \
    var = LoadTexture(path); \
    if ((var.id) == 0){ \
        TraceLog(LOG_ERROR, "Failed to load texture: %s", path); \
        CloseWindow(); \
        return 1; \
    } \
}while(0)

int main(void)
{
    const int W=800, H=450;
    InitWindow(W, H, "raylib + ECS: coins, vendor, hat");
    SetTargetFPS(120);

    input_init_defaults(); // set up logical->physical bindings

    TRY_TEX(tex_player,     "assets/player.png");
    TRY_TEX(tex_player_hat, "assets/player_hat.png");
    TRY_TEX(tex_coin,       "assets/coin.png");
    TRY_TEX(tex_npc,        "assets/npc.png");

    // Player
    ecs_entity_t player = ecs_create();
    cmp_add_position(player, W/2.0f, H/2.0f);
    cmp_add_velocity(player, 0, 0);
    cmp_add_sprite(player, tex_player,
                   (Rectangle){0,0,(float)tex_player.width,(float)tex_player.height},
                   tex_player.width/2.0f, tex_player.height/2.0f);
    tag_add_player(player);
    cmp_add_inventory(player);
    // size roughly matches sprite half-dims or your old 12 radius
    cmp_add_size(player, 12.0f, 12.0f);

    // Three coins (no colliders needed for pickup)
    const Vector2 coinPos[3] = {
        { W/2.0f + 120.0f, H/2.0f },
        { W/2.0f - 160.0f, H/2.0f - 60.0f },
        { W/2.0f +  40.0f, H/2.0f + 90.0f }
    };
    for(int i=0;i<3;++i){
        ecs_entity_t c = ecs_create();
        cmp_add_position(c, coinPos[i].x, coinPos[i].y);
        cmp_add_sprite(c, tex_coin,
                       (Rectangle){0,0,(float)tex_coin.width,(float)tex_coin.height},
                       tex_coin.width/2.0f, tex_coin.height/2.0f);
        cmp_add_item(c, ITEM_COIN);
        cmp_add_size(c, tex_coin.width * 0.5f, tex_coin.height * 0.5f);
    }

    // NPC vendor
    ecs_entity_t npc = ecs_create();
    cmp_add_position(npc, 100.0f, 160.0f);
    cmp_add_sprite(npc, tex_npc,
                   (Rectangle){0,0,(float)tex_npc.width,(float)tex_npc.height},
                   tex_npc.width/2.0f, tex_npc.height/2.0f);
    cmp_add_vendor(npc, ITEM_HAT, 3);
    // size: use what you had (12x16 half-extents)
    cmp_add_size(npc, 12.0f, 16.0f);

    // Fixed timestep
    const float FIXED_DT = 1.0f/60.0f;
    float acc = 0.0f;

    while(!WindowShouldClose()){
        input_begin_frame(); // poll once per frame

        float frame = GetFrameTime(); if(frame>0.25f) frame=0.25f;
        acc += frame;

        while(acc >= FIXED_DT){
            input_t in = input_for_tick(); // per-tick view (edges to first tick)

            if(ui_toast_timer > 0.0f) ui_toast_timer -= FIXED_DT;
            sys_input(FIXED_DT, &in);
            sys_physics(FIXED_DT);
            sys_bounds(FIXED_DT, W, H);
            sys_proximity(FIXED_DT, &in);
            sys_events();
            acc -= FIXED_DT;
        }

        BeginDrawing();
        ClearBackground((Color){24,24,32,255});

        // World
        sys_render();
        debug_draw_colliders();

        // Contextual interact hint near vendors
        {
            int p = find_player();
            if (p >= 0 && (ecs_mask[p] & CMP_COL)){
                for (int v=0; v<ECS_MAX_ENTITIES; ++v){
                    if(!ecs_alive[v]) continue;

                    // require BOTH CMP_VENDOR and CMP_COL, and skip the player entity
                    if ((ecs_mask[v] & (CMP_VENDOR | CMP_COL)) != (CMP_VENDOR | CMP_COL)) continue;
                    if (v == p) continue;

                    if (col_overlap_padded(p, v, 30.0f)){
                        char msg[64];
                        snprintf(msg, sizeof(msg), "Press E to buy hat (%d)", cmp_vendor[v].price);

                        // OPTION A: draw near the VENDOR (current behavior)
                        int tw = MeasureText(msg, 18);
                        int x = (int)(cmp_pos[v].x - tw/2), y = (int)(cmp_pos[v].y - 32);

                        // OPTION B: (if you prefer it on the PLAYER)
                        // int tw = MeasureText(msg, 18);
                        // int x = (int)(cmp_pos[p].x - tw/2), y = (int)(cmp_pos[p].y - 40);

                        DrawRectangle(x-6, y-6, tw+12, 26, (Color){0,0,0,160});
                        DrawText(msg, x, y, 18, RAYWHITE);
                        break;
                    }
                }
            }
        }

        // HUD
        char hud[64];
        snprintf(hud, sizeof(hud), "Coins: %d  (%s)",
                 cmp_inv[player].coins, cmp_inv[player].hasHat?"Hat ON":"No hat");
        DrawText(hud, 10, 10, 20, RAYWHITE);
        DrawText("Move: Arrows/WASD | Interact: E/Space", 10, 36, 18, GRAY);

        // Toast
        if(ui_toast_timer > 0.0f){
            int tw = MeasureText(ui_toast_text, 20);
            int x = (W - tw)/2, y = 10;
            DrawRectangle(x-8, y-4, tw+16, 28, (Color){0,0,0,180});
            DrawText(ui_toast_text, x, y, 20, RAYWHITE);
        }

        EndDrawing();
    }

    UnloadTexture(tex_player);
    UnloadTexture(tex_player_hat);
    UnloadTexture(tex_coin);
    UnloadTexture(tex_npc);
    CloseWindow();
    return 0;
}
