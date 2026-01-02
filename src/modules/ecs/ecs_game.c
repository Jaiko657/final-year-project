#include "modules/core/engine_types.h"
#include "modules/renderer/renderer.h"
#include "modules/ecs/ecs.h"
#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs_proximity.h"
#include "modules/ecs/ecs_game.h"
#include "modules/asset/asset.h"
#include "modules/systems/systems.h"
#include "modules/systems/systems_registration.h"
#include "modules/core/toast.h"
#include "modules/core/logger.h"
#include "modules/tiled/tiled.h"
#include "modules/ecs/ecs_prefab_loading.h"
#include "modules/world/world.h"
#include "modules/world/world_renderer.h"
#include "modules/ecs/ecs_door_systems.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

bool init_entities(const char* tmx_path)
{
    const world_map_t* map = world_get_map();
    if (!map) {
        LOGC(LOGCAT_ECS, LOG_LVL_ERROR, "init_entities: no tiled map loaded");
        return false;
    }

    ecs_prefab_spawn_from_map(map, tmx_path);
    return true;
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

bool ecs_game_get_item(ecs_entity_t e, int* out_kind)
{
    int i = ent_index_checked(e);
    if (i < 0) return false;
    if ((ecs_mask[i] & CMP_ITEM) == 0) return false;
    if (out_kind) *out_kind = (int)g_item[i].kind;
    return true;
}

bool ecs_game_get_inventory(ecs_entity_t e, int* out_coins, bool* out_has_hat)
{
    int i = ent_index_checked(e);
    if (i < 0) return false;
    if ((ecs_mask[i] & CMP_INV) == 0) return false;
    if (out_coins) *out_coins = g_inv[i].coins;
    if (out_has_hat) *out_has_hat = g_inv[i].hasHat;
    return true;
}

bool ecs_game_get_vendor(ecs_entity_t e, int* out_sells, int* out_price)
{
    int i = ent_index_checked(e);
    if (i < 0) return false;
    if ((ecs_mask[i] & CMP_VENDOR) == 0) return false;
    if (out_sells) *out_sells = (int)g_vendor[i].sells;
    if (out_price) *out_price = g_vendor[i].price;
    return true;
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

                tex_handle_t hatTexture = asset_acquire_texture("assets/images/player_hat.png");
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
SYSTEMS_ADAPT_VOID(sys_pickups_adapt, sys_pickups_from_proximity_impl)
SYSTEMS_ADAPT_INPUT(sys_interact_adapt, sys_interact_from_proximity_impl)

void ecs_register_game_systems(void)
{
    // maintain original ordering around billboards
    systems_register(PHASE_SIM_POST, 100, sys_pickups_adapt, "pickups");
    systems_register(PHASE_SIM_POST, 300, sys_interact_adapt, "interact");
    ecs_register_door_systems();
}
