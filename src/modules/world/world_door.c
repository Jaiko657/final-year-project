#include "modules/world/world_door.h"
#include "modules/world/world_map.h"
#include "modules/world/world_renderer.h"
#include "modules/common/dynarray.h"

#include <stdbool.h>
#include <stdlib.h>

typedef struct {
    int layer_idx;
    int tileset_idx;
    int base_tile_id;
    uint32_t flip_flags;
} world_door_tile_info_t;

typedef struct {
    door_tile_xy_t coord;
    world_door_tile_info_t info;
} world_door_tile_t;

typedef struct {
    bool active;
    world_door_tile_t* tiles;
    size_t tile_count;
    uint32_t resolved_map_gen;
    int primary_anim_total_ms;
} world_door_record_t;

static DA(world_door_record_t) g_world_doors = {0};

static world_door_record_t* record_from_handle(world_door_handle_t handle)
{
    if (!handle) return NULL;
    size_t idx = (size_t)(handle - 1);
    if (idx >= g_world_doors.size) return NULL;
    world_door_record_t* rec = &g_world_doors.data[idx];
    return rec->active ? rec : NULL;
}

static int door_frame_at(const tiled_tileset_t* ts, int base_tile, float t_ms, bool opening)
{
    if (!ts || base_tile < 0 || base_tile >= ts->tilecount) return base_tile;
    tiled_animation_t* anim = ts->anims ? &ts->anims[base_tile] : NULL;
    if (!anim || anim->frame_count == 0 || anim->total_duration_ms <= 0) return base_tile;

    int frame = base_tile;
    if (opening) {
        int acc = 0;
        for (size_t i = 0; i < anim->frame_count; ++i) {
            acc += anim->frames[i].duration_ms;
            if (t_ms < acc) { frame = anim->frames[i].tile_id; break; }
        }
        if (t_ms >= anim->total_duration_ms) frame = anim->frames[anim->frame_count - 1].tile_id;
    } else {
        int acc = 0;
        for (size_t i = anim->frame_count; i-- > 0; ) {
            acc += anim->frames[i].duration_ms;
            if (t_ms < acc) { frame = anim->frames[i].tile_id; break; }
        }
        if (t_ms >= anim->total_duration_ms) frame = anim->frames[0].tile_id;
    }
    return frame;
}

static bool resolve_record(world_door_record_t* rec)
{
    if (!rec || rec->tile_count == 0) return false;
    const world_map_t* map = world_get_map();
    if (!map) return false;

    uint32_t current_gen = world_map_generation();
    if (rec->resolved_map_gen == current_gen && rec->tiles) return true;

    const uint32_t FLIP_MASK = 0xE0000000u;
    const uint32_t GID_MASK = TILED_GID_MASK;

    for (size_t i = 0; i < rec->tile_count; ++i) {
        world_door_tile_t* tile = &rec->tiles[i];
        tile->info.layer_idx = -1;
        tile->info.tileset_idx = -1;
        tile->info.base_tile_id = -1;
        tile->info.flip_flags = 0;

        int tx = tile->coord.x;
        int ty = tile->coord.y;
        uint32_t raw_gid = 0;
        for (size_t li = map->layer_count; li-- > 0; ) {
            const tiled_layer_t* layer = &map->layers[li];
            if (!layer->gids) continue;
            if (tx < 0 || ty < 0 || tx >= layer->width || ty >= layer->height) continue;
            uint32_t gid = layer->gids[(size_t)ty * (size_t)layer->width + (size_t)tx];
            if (gid == 0) continue;
            raw_gid = gid;
            tile->info.layer_idx = (int)li;
            break;
        }
        if (raw_gid == 0) continue;

        tile->info.flip_flags = raw_gid & FLIP_MASK;
        uint32_t bare_gid = raw_gid & GID_MASK;
        int chosen_ts = -1;
        int local_id = 0;
        for (size_t si = 0; si < map->tileset_count; ++si) {
            const tiled_tileset_t* ts = &map->tilesets[si];
            int local = (int)bare_gid - ts->first_gid;
            if (local < 0 || local >= ts->tilecount) continue;
            chosen_ts = (int)si;
            local_id = local;
            break;
        }
        tile->info.tileset_idx = chosen_ts;
        tile->info.base_tile_id = local_id;
    }

    rec->primary_anim_total_ms = 0;
    if (rec->tile_count > 0) {
        world_door_tile_t* primary = &rec->tiles[0];
        if (primary->info.tileset_idx >= 0 && primary->info.tileset_idx < (int)map->tileset_count) {
            const tiled_tileset_t* ts = &map->tilesets[primary->info.tileset_idx];
            int base_tile = primary->info.base_tile_id;
            tiled_animation_t* anim = ts->anims && base_tile >= 0 && base_tile < ts->tilecount
                ? &ts->anims[base_tile] : NULL;
            if (anim) {
                rec->primary_anim_total_ms = anim->total_duration_ms;
            }
        }
    }

    rec->resolved_map_gen = current_gen;
    return true;
}

static void apply_record(world_door_record_t* rec, float t_ms, bool play_forward)
{
    if (!rec || !rec->tiles) return;
    const world_map_t* map = world_get_map();
    if (!map) return;

    for (size_t i = 0; i < rec->tile_count; ++i) {
        world_door_tile_t* tile = &rec->tiles[i];
        int li = tile->info.layer_idx;
        int tsi = tile->info.tileset_idx;
        int base = tile->info.base_tile_id;
        if (li < 0 || tsi < 0 || base < 0) continue;
        if ((size_t)li >= map->layer_count) continue;
        if ((size_t)tsi >= map->tileset_count) continue;
        const tiled_tileset_t* ts = &map->tilesets[(size_t)tsi];
        const tiled_layer_t* layer = &map->layers[(size_t)li];
        if (!ts || !layer || !layer->gids) continue;
        int frame_tile = door_frame_at(ts, base, t_ms, play_forward);
        int tx = tile->coord.x;
        int ty = tile->coord.y;
        if (tx < 0 || ty < 0 || tx >= layer->width || ty >= layer->height) continue;
        uint32_t gid = (uint32_t)(ts->first_gid + frame_tile) | tile->info.flip_flags;
        world_set_tile_gid(li, tx, ty, gid);
    }
}

world_door_handle_t world_door_register(const door_tile_xy_t* tile_xy, size_t tile_count)
{
    if (!tile_xy || tile_count == 0) return WORLD_DOOR_INVALID_HANDLE;

    size_t idx = 0;
    for (; idx < g_world_doors.size; ++idx) {
        if (!g_world_doors.data[idx].active) break;
    }

    if (idx >= g_world_doors.size) {
        world_door_record_t blank = {0};
        DA_APPEND(&g_world_doors, blank);
    }

    world_door_record_t* rec = &g_world_doors.data[idx];
    rec->active = true;
    rec->tile_count = tile_count;
    rec->resolved_map_gen = 0;
    rec->primary_anim_total_ms = 0;
    free(rec->tiles);
    rec->tiles = (world_door_tile_t*)malloc(tile_count * sizeof(world_door_tile_t));
    if (!rec->tiles) {
        rec->active = false;
        rec->tile_count = 0;
        return WORLD_DOOR_INVALID_HANDLE;
    }

    for (size_t i = 0; i < tile_count; ++i) {
        rec->tiles[i].coord = tile_xy[i];
        rec->tiles[i].info.layer_idx = -1;
        rec->tiles[i].info.tileset_idx = -1;
        rec->tiles[i].info.base_tile_id = -1;
        rec->tiles[i].info.flip_flags = 0;
    }

    return (world_door_handle_t)(idx + 1);
}

void world_door_unregister(world_door_handle_t handle)
{
    world_door_record_t* rec = record_from_handle(handle);
    if (!rec) return;
    free(rec->tiles);
    rec->tiles = NULL;
    rec->tile_count = 0;
    rec->resolved_map_gen = 0;
    rec->primary_anim_total_ms = 0;
    rec->active = false;
}

int world_door_primary_animation_duration(world_door_handle_t handle)
{
    world_door_record_t* rec = record_from_handle(handle);
    if (!rec) return 0;
    if (!resolve_record(rec)) return 0;
    return rec->primary_anim_total_ms;
}

void world_door_apply_state(world_door_handle_t handle, float t_ms, bool play_forward)
{
    world_door_record_t* rec = record_from_handle(handle);
    if (!rec) return;
    if (!resolve_record(rec)) return;
    apply_record(rec, t_ms, play_forward);
}

void world_door_shutdown(void)
{
    for (size_t i = 0; i < g_world_doors.size; ++i) {
        if (!g_world_doors.data[i].active) continue;
        free(g_world_doors.data[i].tiles);
        g_world_doors.data[i].tiles = NULL;
        g_world_doors.data[i].tile_count = 0;
        g_world_doors.data[i].active = false;
    }
    DA_FREE(&g_world_doors);
}
