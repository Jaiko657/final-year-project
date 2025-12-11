#include "../includes/engine_types.h"
#include "../includes/renderer.h"
#include "../includes/ecs.h"
#include "../includes/ecs_internal.h"
#include "../includes/ecs_proximity.h"
#include "../includes/ecs_game.h"
#include "../includes/asset.h"
#include "../includes/logger.h"
#include "../includes/world.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

int init_entities(int W, int H)
{
    tex_handle_t tex_player     = asset_acquire_texture("assets/player.png");
    tex_handle_t tex_coin       = asset_acquire_texture("assets/coin_gold.png");
    tex_handle_t tex_npc        = asset_acquire_texture("assets/npc.png");

    int pw=16, ph=16, cw=32, ch=32, nw=16, nh=16;
    v2f spawn = world_get_spawn_px();

    // ADD PLAYER
    ecs_entity_t player = ecs_create();
    cmp_add_position(player, spawn.x, spawn.y);
    cmp_add_velocity(player, 0, 0, DIR_SOUTH);
    cmp_add_sprite_handle(player, tex_player,
        (rectf){0,16,(float)pw,(float)ph},
        pw*0.5f, ph*0.5f);
    cmp_add_player(player);
    cmp_add_inventory(player);
    cmp_add_size(player, pw/2, ph/2);
    cmp_add_phys_body_default(player, PHYS_DYNAMIC);
    cmp_add_trigger(player, 0.0f, CMP_ITEM | CMP_COL);

    enum {
        ANIM_WALK_N = 0,
        ANIM_WALK_NE,
        ANIM_WALK_E,
        ANIM_WALK_SE,
        ANIM_WALK_S,
        ANIM_WALK_SW,
        ANIM_WALK_W,
        ANIM_WALK_NW,

        ANIM_IDLE_N,
        ANIM_IDLE_NE,
        ANIM_IDLE_E,
        ANIM_IDLE_SE,
        ANIM_IDLE_S,
        ANIM_IDLE_SW,
        ANIM_IDLE_W,
        ANIM_IDLE_NW,

        ANIM_COUNT
    };
    int frames_per_anim[ANIM_COUNT] = {
        2,2,2,2,2,2,2,2,
        1,1,1,1,1,1,1,1
    };
    anim_frame_coord_t anim_frames[ANIM_COUNT][MAX_FRAMES] = {
        //WALKING
        { {0,0}, {0,2} },
        { {1,0}, {1,2} },
        { {2,0}, {2,2} },
        { {3,0}, {3,2} },
        { {4,0}, {4,2} },
        { {5,0}, {5,2} },
        { {6,0}, {6,2} },
        { {7,0}, {7,2} },
        //IDLE
        { {0,1} },
        { {1,1} },
        { {2,1} },
        { {3,1} },
        { {4,1} },
        { {5,1} },
        { {6,1} },
        { {7,1} },
    };
    cmp_add_anim(
        player,
        16,
        16,
        ANIM_COUNT,
        frames_per_anim,
        anim_frames,
        6.0f
    );

    // ADD COINS
    const v2f coinPos[3] = {
        { W/2.0f + 120.0f, H/2.0f },
        { W/2.0f, H/2.0f - 60.0f },
        { W/2.0f - 40.0f, H/2.0f + 50.0f }
    };
    for(int i=0;i<3;++i){
        ecs_entity_t c = ecs_create();
        cmp_add_position(c, coinPos[i].x, coinPos[i].y);
        cmp_add_sprite_handle(c, tex_coin,
            (rectf){0,0,(float)cw,(float)ch},
            cw*0.5f, ch*0.5f);
        cmp_add_item(c, ITEM_COIN);
        cmp_add_size(c, cw * 0.25f, ch * 0.25f);

        int frames_per_anim_coin[1] = { 8 };
        anim_frame_coord_t anim_frames_coin[1][MAX_FRAMES] = {
            { {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5,0},{6, 0}, {7,0} },
        };
        cmp_add_anim(
            c,
            cw,
            ch,
            1,
            frames_per_anim_coin,
            anim_frames_coin,
            8.0f
        );
    }

    // ADD NPC/VENDOR
    ecs_entity_t npc = ecs_create();
    cmp_add_position(npc, 100.0f, H/2.0f);
    cmp_add_sprite_handle(npc, tex_npc,
        (rectf){0,0,(float)nw,(float)nh},
        nw*0.5f, nh*0.5f);
    cmp_add_velocity(npc, 0.0f, 0.0f, DIR_SOUTH);
    cmp_add_vendor(npc, ITEM_HAT, 3);
    cmp_add_size(npc, 6.0f, 6.0f);
    cmp_add_liftable(npc);
    cmp_add_phys_body_default(npc, PHYS_DYNAMIC);

    cmp_add_trigger(npc, 30.0f, CMP_PLAYER | CMP_COL);
    cmp_add_billboard(npc, "Press E to buy hat", -16.0f, 0.10f, BILLBOARD_ACTIVE);

    // Release setup refs (ECS added its own refs)
    asset_release_texture(tex_player);
    asset_release_texture(tex_coin);
    asset_release_texture(tex_npc);
    return 0;
}

// ===== Game-side component storage =====
typedef struct { item_kind_t kind; } cmp_item_t;
typedef struct { int coins; bool hasHat; } cmp_inventory_t;
typedef struct { item_kind_t sells; int price; } cmp_vendor_t;

static cmp_item_t      g_item[ECS_MAX_ENTITIES];
static cmp_inventory_t g_inv[ECS_MAX_ENTITIES];
static cmp_vendor_t    g_vendor[ECS_MAX_ENTITIES];

// ===== Component adders =====
void cmp_add_inventory(ecs_entity_t e)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    g_inv[i] = (cmp_inventory_t){ 0, false };
    ecs_mask[i] |= CMP_INV;
}

void cmp_add_item(ecs_entity_t e, item_kind_t k)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    g_item[i] = (cmp_item_t){ k };
    ecs_mask[i] |= CMP_ITEM;
}

void cmp_add_vendor(ecs_entity_t e, item_kind_t sells, int price)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    g_vendor[i] = (cmp_vendor_t){ sells, price };
    ecs_mask[i] |= CMP_VENDOR;
}

// ===== Gameplay helpers =====
void game_get_player_stats(int* outCoins, bool* outHasHat)
{
    ecs_entity_t handle = find_player_handle();
    int p = ent_index_checked(handle);

    int coins = 0; bool hat = false;
    if (p >= 0 && (ecs_mask[p] & CMP_INV)) {
        coins = g_inv[p].coins;
        hat   = g_inv[p].hasHat;
    }
    if (outCoins)  *outCoins  = coins;
    if (outHasHat) *outHasHat = hat;
}

bool game_vendor_hint_is_active(int* out_x, int* out_y, const char** out_text)
{
    ecs_entity_t h = find_player_handle();
    int p = ent_index_checked(h);
    if (p < 0 || !(ecs_mask[p] & CMP_COL)) return false;

    for (int v = 0; v < ECS_MAX_ENTITIES; ++v) {
        if (!ecs_alive_idx(v)) continue;
        if ((ecs_mask[v] & (CMP_VENDOR | CMP_COL)) != (CMP_VENDOR | CMP_COL)) continue;
        if (v == p) continue;

        // simple padded AABB overlap
        float ax = cmp_pos[p].x, ay = cmp_pos[p].y;
        float bx = cmp_pos[v].x, by = cmp_pos[v].y;
        float ahx = cmp_col[p].hx, ahy = cmp_col[p].hy;
        float bhx = cmp_col[v].hx, bhy = cmp_col[v].hy;
        float pad = 30.0f;

        if (fabsf(ax - bx) <= (ahx + bhx + pad) &&
            fabsf(ay - by) <= (ahy + bhy + pad))
        {
            static char buf[64];
            snprintf(buf, sizeof(buf), "Press E to buy hat (%d)", g_vendor[v].price);
            if (out_text) *out_text = buf;
            if (out_x)    *out_x    = (int)cmp_pos[v].x;
            if (out_y)    *out_y    = (int)cmp_pos[v].y;
            return true;
        }
    }
    return false;
}

// ===== Systems: pickups & vendor interaction =====
static void sys_pickups_from_proximity_impl(void)
{
    ecs_prox_iter_t it = ecs_prox_enter_begin();
    ecs_prox_view_t v;
    while (ecs_prox_enter_next(&it, &v)) {
        int ia = ent_index_checked(v.trigger_owner);
        int ib = ent_index_checked(v.matched_entity);
        if (ia < 0 || ib < 0) continue;

        if ((ecs_mask[ia] & CMP_INV) &&
            (ecs_mask[ib] & CMP_ITEM) &&
            g_item[ib].kind == ITEM_COIN)
        {
            g_inv[ia].coins += 1;
            ecs_destroy(v.matched_entity);
            ui_toast(1.0f, "Picked up a coin! (%d)", g_inv[ia].coins);
        }
    }
}

static void try_buy_hat(ecs_entity_t player, ecs_entity_t vendor)
{
    int ia = ent_index_checked(player);
    int ib = ent_index_checked(vendor);
    if ((ecs_mask[ib] & CMP_VENDOR) == 0) return;
    if ((ecs_mask[ia] & CMP_INV) == 0) return;

    cmp_vendor_t v  = g_vendor[ib];
    cmp_inventory_t* pInv = &g_inv[ia];
    
    LOGC(LOGCAT_ECS, LOG_LVL_DEBUG, "HELLO");
    if (v.sells == ITEM_HAT) {
        if (pInv->hasHat) {
            ui_toast(1.5f, "You already have a hat.");
            return;
        }
        if (pInv->coins >= v.price) {
            pInv->coins -= v.price;
            pInv->hasHat = true;

            if ((ecs_mask[ia] & CMP_SPR)) {
                // Collect at end of frame so isnt an issue freeing first
                if (asset_texture_valid(cmp_spr[ia].tex))
                    asset_release_texture(cmp_spr[ia].tex);

                tex_handle_t hatTexture = asset_acquire_texture("assets/player_hat.png");
                cmp_spr[ia].tex = hatTexture;

                int w = 16, h = 16;
                cmp_spr[ia].src = rectf_xywh(0, 0, (float)w, (float)h);
                cmp_spr[ia].ox  = w * 0.5f;
                cmp_spr[ia].oy  = h * 0.5f;
            }
            ui_toast(1.5f, "Bought hat for %d coins!", v.price);
            cmp_billboard[ib].state = BILLBOARD_INACTIVE;
            //REMOVES HAT
            cmp_spr[ib].src.y += 16;

            cmp_add_follow(vendor, player, 30.0f, 90.0f);

        } else {
            ui_toast(1.5f, "Need %d coins for hat (you have %d).",
                     v.price, pInv->coins);
        }
    }
}

static void sys_interact_from_proximity_impl(const input_t* in)
{
    if (!input_pressed(in, BTN_INTERACT)) return;

    ecs_entity_t player_h = find_player_handle();
    int p = ent_index_checked(player_h);
    if (p < 0 || !(ecs_mask[p] & (CMP_COL | CMP_POS))) return;

    float best_d2 = INFINITY;
    int   best_vendor = -1;

    ecs_prox_iter_t it = ecs_prox_stay_begin();
    ecs_prox_view_t v;

    while (ecs_prox_stay_next(&it, &v)) {
        int ia = ent_index_checked(v.trigger_owner);
        int ib = ent_index_checked(v.matched_entity);
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

        if ((ecs_mask[vendor] & CMP_POS) == 0 ||
            (ecs_mask[player] & CMP_POS) == 0) continue;

        float dx = cmp_pos[vendor].x - cmp_pos[player].x;
        float dy = cmp_pos[vendor].y - cmp_pos[player].y;
        float d2 = dx*dx + dy*dy;

        if (d2 < best_d2) {
            best_d2    = d2;
            best_vendor = vendor;
        }
    }

    if (best_vendor >= 0) {
        try_buy_hat(player_h, handle_from_index(best_vendor));
    }
}

// ===== System adapters + registration =====
static void sys_pickups_adapt(float dt, const input_t* in)
{
    (void)dt; (void)in;
    sys_pickups_from_proximity_impl();
}

static void sys_interact_adapt(float dt, const input_t* in)
{
    (void)dt;
    sys_interact_from_proximity_impl(in);
}

void ecs_register_game_systems(void)
{
    // maintain original ordering around billboards
    ecs_register_system(PHASE_SIM_POST, 100, sys_pickups_adapt, "pickups");
    ecs_register_system(PHASE_SIM_POST, 300, sys_interact_adapt, "interact");
}
