#include "../includes/engine_types.h"
#include "../includes/renderer.h"
#include "../includes/ecs.h"
#include "../includes/ecs_internal.h"
#include "../includes/ecs_proximity.h"
#include "../includes/ecs_game.h"
#include "../includes/asset.h"
#include "../includes/logger.h"
#include "../includes/tiled.h"
#include "../includes/prefab.h"
#include "../includes/dynarray.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef DA(door_tile_xy_t) door_tile_xy_da_t;

static int collect_door_tiles(const tiled_map_t* map, const tiled_object_t* obj, door_tile_xy_da_t* tiles_xy)
{
    if (!tiles_xy) return 0;
    DA_CLEAR(tiles_xy);
    int tile_count = 0;
    if (obj->door_tile_count > 0) {
        DA_RESERVE(tiles_xy, obj->door_tile_count);
        for (int t = 0; t < obj->door_tile_count; ++t) {
            DA_APPEND(tiles_xy, ((door_tile_xy_t){ obj->door_tiles[t][0], obj->door_tiles[t][1] }));
        }
    } else {
        float ow = (obj->w > 0.0f) ? obj->w : (float)map->tilewidth;
        float oh = (obj->h > 0.0f) ? obj->h : (float)map->tileheight;
        float top = obj->y - oh;
        float left = obj->x;
        int start_tx = (int)floorf(left / map->tilewidth);
        int end_tx   = (int)ceilf((left + ow) / map->tilewidth);
        int start_ty = (int)floorf(top / map->tileheight);
        int end_ty   = (int)ceilf((top + oh) / map->tileheight);
        for (int ty = start_ty; ty < end_ty; ++ty) {
            for (int tx = start_tx; tx < end_tx; ++tx) {
                DA_APPEND(tiles_xy, ((door_tile_xy_t){ tx, ty }));
            }
        }
    }
    tile_count = (int)tiles_xy->size;
    return tile_count;
}

static void resolve_door_tiles(const tiled_map_t* map, cmp_door_t* door)
{
    if (!map || !door) return;
    const uint32_t FLIP_H = 0x80000000;
    const uint32_t FLIP_V = 0x40000000;
    const uint32_t FLIP_D = 0x20000000;
    const uint32_t GID_MASK = 0x1FFFFFFF;

    for (size_t t = 0; t < door->tiles.size; ++t) {
        int tx = door->tiles.data[t].x;
        int ty = door->tiles.data[t].y;
        uint32_t raw_gid = 0;
        int found_layer = -1;
        // Prefer the topmost layer that contains a non-empty tile at this location.
        for (size_t li = map->layer_count; li-- > 0; ) {
            const tiled_layer_t *layer = &map->layers[li];
            if (tx < 0 || ty < 0 || tx >= layer->width || ty >= layer->height) continue;
            uint32_t gid = layer->gids[(size_t)ty * (size_t)layer->width + (size_t)tx];
            if (gid == 0) continue;
            raw_gid = gid;
            found_layer = (int)li;
            break;
        }
        door->tiles.data[t].layer_idx = found_layer;
        door->tiles.data[t].flip_flags = raw_gid & (FLIP_H | FLIP_V | FLIP_D);
        uint32_t bare_gid = raw_gid & GID_MASK;
        int chosen_ts = -1;
        int local_id = 0;
        for (size_t si = 0; si < map->tileset_count; ++si) {
            const tiled_tileset_t *ts = &map->tilesets[si];
            int local = (int)bare_gid - ts->first_gid;
            if (local < 0 || local >= ts->tilecount) continue;
            chosen_ts = (int)si;
            local_id = local;
        }
        door->tiles.data[t].tileset_idx = chosen_ts;
        door->tiles.data[t].base_tile_id = local_id;
    }
}

static void spawn_doors_from_map(const tiled_map_t* map)
{
    if (!map) return;
    door_tile_xy_da_t tiles_xy = {0};
    for (size_t i = 0; i < map->object_count; ++i) {
        const tiled_object_t *obj = &map->objects[i];
        if (!obj->animationtype) continue;
        if (strcmp(obj->animationtype, "proximity-door") != 0 && strcmp(obj->animationtype, "door") != 0) continue;

        float ow = (obj->w > 0.0f) ? obj->w : (float)map->tilewidth;
        float oh = (obj->h > 0.0f) ? obj->h : (float)map->tileheight;
        // Tiled object y is bottom by default for gid objects; place at object center
        float px = obj->x + ow * 0.5f;
        float py = obj->y - oh * 0.5f;

        float prox_r = (obj->proximity_radius > 0) ? (float)obj->proximity_radius : 64.0f;
        float prox_ox = (float)obj->proximity_off_x;
        float prox_oy = (float)obj->proximity_off_y;

        // Determine which tiles this door covers: prefer explicit door_tiles property, fallback to size
        DA_CLEAR(&tiles_xy);
        int tile_count = collect_door_tiles(map, obj, &tiles_xy);

        ecs_entity_t door = ecs_create();
        cmp_add_position(door, px, py);
        cmp_add_size(door, ow * 0.5f, oh * 0.5f); // gives CMP_COL for proximity system
        cmp_add_trigger(door, prox_r, CMP_PLAYER | CMP_COL);
        cmp_add_door(door,
                     prox_r,
                     prox_ox,
                     prox_oy,
                     tile_count,
                     tile_count > 0 ? tiles_xy.data : NULL);

        // store extra tile info directly in cmp_door
        int di = ent_index_checked(door);
        if (di >= 0) {
            resolve_door_tiles(map, &cmp_door[di]);
        }
    }
    DA_FREE(&tiles_xy);
}

bool init_entities(const char* tmx_path)
{
    const tiled_map_t* map = renderer_get_tiled_map();
    if (!map) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "init_entities: no tiled map loaded");
        return false;
    }

    prefab_spawn_from_map(map, tmx_path);
    spawn_doors_from_map(map);
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

static void sys_doors_present(float dt);

static void sys_doors_present_adapt(float dt, const input_t* in)
{
    (void)in;
    sys_doors_present(dt);
}

static const tiled_tileset_t* ts_for_index(const tiled_map_t* map, int idx) {
    if (!map || idx < 0 || (size_t)idx >= map->tileset_count) return NULL;
    return &map->tilesets[idx];
}

static int door_frame_at(const tiled_tileset_t* ts, int base_tile, float t_ms, bool opening)
{
    if (!ts || base_tile < 0 || base_tile >= ts->tilecount) return base_tile;
    tiled_animation_t *anim = ts->anims ? &ts->anims[base_tile] : NULL;
    if (!anim || anim->frame_count == 0 || anim->total_duration_ms <= 0) return base_tile;

    int frame = base_tile;
    if (opening) {
        int acc = 0;
        for (size_t i = 0; i < anim->frame_count; ++i) {
            acc += anim->frames[i].duration_ms;
            if (t_ms < acc) { frame = anim->frames[i].tile_id; break; }
        }
        if (t_ms >= anim->total_duration_ms) {
            frame = anim->frames[anim->frame_count - 1].tile_id;
        }
    } else {
        int acc = 0;
        for (size_t i = anim->frame_count; i-- > 0; ) {
            acc += anim->frames[i].duration_ms;
            if (t_ms < acc) { frame = anim->frames[i].tile_id; break; }
        }
        if (t_ms >= anim->total_duration_ms) {
            frame = anim->frames[0].tile_id;
        }
    }
    return frame;
}

static void sys_doors_present(float dt)
{
    tiled_map_t* map = renderer_get_tiled_map();
    if (!map) return;

    const uint32_t FLIP_MASK = 0xE0000000;

    // Build intent from proximity stay/enter
    bool door_should_open[ECS_MAX_ENTITIES] = {0};
    for (ecs_prox_iter_t it = ecs_prox_stay_begin(); ; ) {
        ecs_prox_view_t v;
        if (!ecs_prox_stay_next(&it, &v)) break;
        int a = ent_index_checked(v.trigger_owner);
        if (a >= 0 && (ecs_mask[a] & CMP_DOOR)) {
            door_should_open[a] = true;
        }
    }
    for (ecs_prox_iter_t it = ecs_prox_enter_begin(); ; ) {
        ecs_prox_view_t v;
        if (!ecs_prox_enter_next(&it, &v)) break;
        int a = ent_index_checked(v.trigger_owner);
        if (a >= 0 && (ecs_mask[a] & CMP_DOOR)) {
            door_should_open[a] = true;
        }
    }

    float dt_ms = dt * 1000.0f;
    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & CMP_DOOR) != CMP_DOOR) continue;
        cmp_door_t *d = &cmp_door[i];
        bool intent_open = door_should_open[i];

        // Use first tile's animation (if any) to clamp state transitions
        size_t tile_count = d->tiles.size;
        int primary_ts = (tile_count > 0) ? d->tiles.data[0].tileset_idx : -1;
        int primary_base = (tile_count > 0) ? d->tiles.data[0].base_tile_id : -1;
        const tiled_tileset_t *prim_ts = ts_for_index(map, primary_ts);
        const tiled_animation_t *prim_anim = (prim_ts && prim_ts->anims && primary_base >= 0 && primary_base < prim_ts->tilecount)
            ? &prim_ts->anims[primary_base] : NULL;
        int primary_total = prim_anim ? prim_anim->total_duration_ms : 0;

        // Update state machine. If reversing direction mid-animation, mirror remaining time
        // so the animation plays smoothly instead of restarting.
        float prev_t = d->anim_time_ms;
        if (intent_open) {
            if (d->state == DOOR_CLOSED) {
                d->anim_time_ms = 0.0f;
                d->state = DOOR_OPENING;
            } else if (d->state == DOOR_CLOSING) {
                if (primary_total > 0) {
                    float clamped = prev_t;
                    if (clamped > (float)primary_total) clamped = (float)primary_total;
                    d->anim_time_ms = (float)primary_total - clamped;
                } else {
                    d->anim_time_ms = 0.0f;
                }
                d->state = DOOR_OPENING;
            }
        } else {
            if (d->state == DOOR_OPEN) {
                d->anim_time_ms = 0.0f;
                d->state = DOOR_CLOSING;
            } else if (d->state == DOOR_OPENING) {
                if (primary_total > 0) {
                    float clamped = prev_t;
                    if (clamped > (float)primary_total) clamped = (float)primary_total;
                    d->anim_time_ms = (float)primary_total - clamped;
                } else {
                    d->anim_time_ms = 0.0f;
                }
                d->state = DOOR_CLOSING;
            }
        }

        if (d->state == DOOR_OPENING || d->state == DOOR_CLOSING) {
            d->anim_time_ms += dt_ms;
        }

        // Derive playback direction/time from actual state (not intent) so idle closed state shows frame 0.
        float t_ms = d->anim_time_ms;
        bool play_forward = true;
        switch (d->state) {
            case DOOR_OPENING:
                play_forward = true;
                break;
            case DOOR_OPEN:
                play_forward = true;
                if (primary_total > 0) t_ms = (float)primary_total;
                break;
            case DOOR_CLOSING:
                play_forward = false;
                break;
            case DOOR_CLOSED:
            default:
                play_forward = true;
                t_ms = 0.0f;
                break;
        }
        if (primary_total > 0 && t_ms > (float)primary_total) {
            t_ms = (float)primary_total;
        }

        // Apply frames to tiles
        for (size_t t = 0; t < d->tiles.size; ++t) {
            door_tile_info_t *info = &d->tiles.data[t];
            int li = info->layer_idx;
            int tsi = info->tileset_idx;
            int base_tile = info->base_tile_id;
            if (li < 0 || tsi < 0) continue;
            if ((size_t)li >= map->layer_count || (size_t)tsi >= map->tileset_count) continue;
            tiled_layer_t *layer = &map->layers[li];
            const tiled_tileset_t *ts = ts_for_index(map, tsi);
            if (!ts) continue;
            int frame_tile = door_frame_at(ts, base_tile, t_ms, play_forward);
            int tx = info->x;
            int ty = info->y;
            if (tx < 0 || ty < 0 || tx >= layer->width || ty >= layer->height) continue;
            size_t idx = (size_t)ty * (size_t)layer->width + (size_t)tx;
            uint32_t gid = (uint32_t)(ts->first_gid + frame_tile) | (info->flip_flags & FLIP_MASK);
            layer->gids[idx] = gid;
        }

        // Clamp final state when animation completes
        if (d->state == DOOR_OPENING) {
            if (primary_total == 0 || d->anim_time_ms >= (float)primary_total) {
                d->state = DOOR_OPEN;
                d->anim_time_ms = (float)primary_total;
            }
        } else if (d->state == DOOR_CLOSING) {
            if (primary_total == 0 || d->anim_time_ms >= (float)primary_total) {
                d->state = DOOR_CLOSED;
                d->anim_time_ms = 0.0f;
            }
        }
    }
}

void ecs_register_game_systems(void)
{
    // maintain original ordering around billboards
    ecs_register_system(PHASE_SIM_POST, 100, sys_pickups_adapt, "pickups");
    ecs_register_system(PHASE_SIM_POST, 300, sys_interact_adapt, "interact");
    ecs_register_system(PHASE_PRESENT, 50, sys_doors_present_adapt, "doors_present");
}
