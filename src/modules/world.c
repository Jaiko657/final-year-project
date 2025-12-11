#include "../includes/world.h"
#include "../includes/tiled.h"
#include "../includes/logger.h"
#include "raylib.h"

#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define WORLD_TILE_SIZE 32
#define WORLD_SUBTILE_SIZE 8
#define WORLD_SUBTILES_PER_TILE (WORLD_TILE_SIZE / WORLD_SUBTILE_SIZE)
#define WORLD_SUBTILES_PER_TILE_TOTAL (WORLD_SUBTILES_PER_TILE * WORLD_SUBTILES_PER_TILE)
#if (WORLD_TILE_SIZE % WORLD_SUBTILE_SIZE) != 0
#error "WORLD_TILE_SIZE must be divisible by WORLD_SUBTILE_SIZE"
#endif

typedef struct {
    int w, h;          // tiles
    int tile_size;     // pixels per tile
    world_tile_t* tiles;
    uint16_t* subtile_masks;
    v2f spawn;         // pixel position at tile center
} world_state_t;

static world_state_t g_world = { .tile_size = WORLD_TILE_SIZE };

int world_tile_size(void) {
    return g_world.tile_size;
}

int world_subtile_size(void) {
    return WORLD_SUBTILE_SIZE;
}

void world_shutdown(void) {
    free(g_world.tiles);
    free(g_world.subtile_masks);
    g_world = (world_state_t){ .tile_size = WORLD_TILE_SIZE };
}

static uint16_t subtile_full_mask(void) {
    return (uint16_t)((1u << WORLD_SUBTILES_PER_TILE_TOTAL) - 1u);
}

static v2f tile_center_px(int tx, int ty, int tile_size) {
    return v2f_make((tx + 0.5f) * (float)tile_size, (ty + 0.5f) * (float)tile_size);
}

static uint16_t flip_mask_h(uint16_t mask) {
    uint16_t out = 0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            int src_bit = y * 4 + x;
            int dst_bit = y * 4 + (3 - x);
            if (mask & (uint16_t)(1u << src_bit)) {
                out |= (uint16_t)(1u << dst_bit);
            }
        }
    }
    return out;
}

static uint16_t flip_mask_v(uint16_t mask) {
    uint16_t out = 0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            int src_bit = y * 4 + x;
            int dst_bit = (3 - y) * 4 + x;
            if (mask & (uint16_t)(1u << src_bit)) {
                out |= (uint16_t)(1u << dst_bit);
            }
        }
    }
    return out;
}

static bool str_ieq(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        ++a; ++b;
    }
    return *a == '\0' && *b == '\0';
}

bool world_load_from_tmx(const char* tmx_path, const char* collision_layer_name) {
    world_shutdown();

    tiled_map_t map;
    if (!tiled_load_map(tmx_path, &map)) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "world: failed to load TMX '%s'", tmx_path);
        return false;
    }

    if (map.tilewidth != WORLD_TILE_SIZE || map.tileheight != WORLD_TILE_SIZE) {
        LOGC(LOGCAT_MAIN, LOG_LVL_WARN, "world: TMX tile size %dx%d differs from engine tile size %d", map.tilewidth, map.tileheight, WORLD_TILE_SIZE);
    }

    size_t count = (size_t)map.width * (size_t)map.height;
    world_tile_t* tiles = (world_tile_t*)malloc(count * sizeof(world_tile_t));
    uint16_t* masks = (uint16_t*)malloc(count * sizeof(uint16_t));
    if (!tiles || !masks) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "world: out of memory for TMX collision (%d x %d)", map.width, map.height);
        free(tiles);
        free(masks);
        tiled_free_map(&map);
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        tiles[i] = WORLD_TILE_WALKABLE;
        masks[i] = 0;
    }

    const uint32_t FLIPPED_HORIZONTALLY_FLAG = 0x80000000;
    const uint32_t FLIPPED_VERTICALLY_FLAG   = 0x40000000;
    const uint32_t FLIPPED_DIAGONALLY_FLAG   = 0x20000000;
    const uint32_t GID_MASK                  = 0x1FFFFFFF;
    (void)FLIPPED_DIAGONALLY_FLAG;

    for (size_t li = 0; li < map.layer_count; ++li) {
        const tiled_layer_t* layer = &map.layers[li];
        bool collision_layer = layer->collision;
        if (!collision_layer && collision_layer_name && layer->name && strcmp(layer->name, collision_layer_name) == 0) {
            collision_layer = true;
        }
        if (!collision_layer) continue;

        size_t idx = 0;
        for (int y = 0; y < layer->height; ++y) {
            for (int x = 0; x < layer->width; ++x, ++idx) {
                if ((size_t)x >= (size_t)map.width || (size_t)y >= (size_t)map.height) continue;
                uint32_t raw_gid = layer->gids[idx];
                if (raw_gid == 0) continue;

                bool flip_h = (raw_gid & FLIPPED_HORIZONTALLY_FLAG) != 0;
                bool flip_v = (raw_gid & FLIPPED_VERTICALLY_FLAG) != 0;
                uint32_t gid = raw_gid & GID_MASK;
                if (gid == 0) continue;

                const tiled_tileset_t *ts = NULL;
                for (size_t ti = 0; ti < map.tileset_count; ++ti) {
                    const tiled_tileset_t *cand = &map.tilesets[ti];
                    int local = (int)gid - cand->first_gid;
                    if (local < 0 || local >= cand->tilecount) continue;
                    ts = cand;
                    gid = (uint32_t)local;
                    break;
                }
                if (!ts) continue;

                int tile_index = (int)gid;
                uint16_t mask = ts->colliders ? ts->colliders[tile_index] : 0;
                if (flip_h) mask = flip_mask_h(mask);
                if (flip_v) mask = flip_mask_v(mask);

                size_t write_idx = (size_t)y * (size_t)map.width + (size_t)x;
                masks[write_idx] = mask;
                tiles[write_idx] = (mask == subtile_full_mask()) ? WORLD_TILE_SOLID : WORLD_TILE_WALKABLE;
            }
        }
    }

    g_world.w = map.width;
    g_world.h = map.height;
    g_world.tiles = tiles;
    g_world.subtile_masks = masks;
    g_world.tile_size = WORLD_TILE_SIZE;
    g_world.spawn = tile_center_px(g_world.w / 2, g_world.h / 2, g_world.tile_size);

    bool spawn_found = false;
    for (size_t i = 0; i < map.object_count; ++i) {
        const tiled_object_t* obj = &map.objects[i];
        if (!obj->name || !str_ieq(obj->name, "spawn")) continue;
        float ow = obj->w > 0.0f ? obj->w : (float)map.tilewidth;
        float oh = obj->h > 0.0f ? obj->h : (float)map.tileheight;
        g_world.spawn = v2f_make(obj->x + ow * 0.5f, obj->y - oh * 0.5f);
        spawn_found = true;
        break;
    }
    if (!spawn_found) {
        LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "tiled: missing spawn object in '%s' (using map center)", tmx_path);
    }

    tiled_free_map(&map);

    LOGC(LOGCAT_MAIN, LOG_LVL_INFO, "world: loaded collision from TMX '%s' (%dx%d)", tmx_path, g_world.w, g_world.h);
    return true;
}

void world_size_tiles(int* out_w, int* out_h) {
    if (out_w) *out_w = g_world.w;
    if (out_h) *out_h = g_world.h;
}

void world_size_px(int* out_w, int* out_h) {
    int ts = g_world.tile_size;
    if (out_w) *out_w = g_world.w * ts;
    if (out_h) *out_h = g_world.h * ts;
}

world_tile_t world_tile_at(int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= g_world.w || ty >= g_world.h || !g_world.tiles) {
        return WORLD_TILE_VOID;
    }
    return g_world.tiles[ty * g_world.w + tx];
}

bool world_is_walkable_subtile(int sx, int sy) {
    if (sx < 0 || sy < 0) return false;
    if (!g_world.tiles || !g_world.subtile_masks) return false;
    const int subtiles_per_tile = WORLD_SUBTILES_PER_TILE;
    if (subtiles_per_tile <= 0 || g_world.w <= 0 || g_world.h <= 0) return false;

    int tile_x = sx / subtiles_per_tile;
    int tile_y = sy / subtiles_per_tile;
    if (tile_x < 0 || tile_y < 0 || tile_x >= g_world.w || tile_y >= g_world.h) return false;

    int local_x = sx % subtiles_per_tile;
    int local_y = sy % subtiles_per_tile;
    int bit = local_y * subtiles_per_tile + local_x;

    size_t idx = (size_t)tile_y * (size_t)g_world.w + (size_t)tile_x;
    uint16_t mask = g_world.subtile_masks ? g_world.subtile_masks[idx] : 0;
    return (mask & (uint16_t)(1u << bit)) == 0;
}

bool world_is_walkable_px(float x, float y) {
    int ss = world_subtile_size();
    if (ss <= 0 || g_world.w <= 0 || g_world.h <= 0) return false;

    int sx = (int)floorf(x / (float)ss);
    int sy = (int)floorf(y / (float)ss);
    return world_is_walkable_subtile(sx, sy);
}

v2f world_get_spawn_px(void) {
    if (!g_world.tiles) {
        return v2f_make(0.0f, 0.0f);
    }
    return v2f_make(g_world.spawn.x, g_world.spawn.y - 32.0f);
}

static const tiled_tileset_t* tileset_for_gid_runtime(const tiled_map_t* map, uint32_t gid, size_t* out_idx, int* out_local) {
    const tiled_tileset_t* match = NULL;
    size_t mi = 0;
    int local = 0;
    for (size_t i = 0; i < map->tileset_count; ++i) {
        const tiled_tileset_t* ts = &map->tilesets[i];
        int maybe = (int)gid - ts->first_gid;
        if (maybe < 0 || maybe >= ts->tilecount) continue;
        match = ts;
        mi = i;
        local = maybe;
    }
    if (match && out_idx) *out_idx = mi;
    if (match && out_local) *out_local = local;
    return match;
}

static int animated_tile_index_now(const tiled_tileset_t* ts, int base_index) {
    if (!ts || !ts->anims || base_index < 0 || base_index >= ts->tilecount) return base_index;
    tiled_animation_t* anim = &ts->anims[base_index];
    if (!anim || anim->frame_count == 0 || anim->total_duration_ms <= 0) return base_index;

    double now_ms = GetTime() * 1000.0;
    int mod = (int)fmod(now_ms, (double)anim->total_duration_ms);
    int acc = 0;
    for (size_t i = 0; i < anim->frame_count; ++i) {
        acc += anim->frames[i].duration_ms;
        if (mod < acc) {
            int idx = anim->frames[i].tile_id;
            if (idx >= 0 && idx < ts->tilecount) return idx;
            break;
        }
    }
    return base_index;
}

void world_sync_tiled_colliders(const tiled_map_t* map) {
    if (!map || map->width != g_world.w || map->height != g_world.h || !g_world.subtile_masks) return;

    const uint32_t FLIPPED_HORIZONTALLY_FLAG = 0x80000000;
    const uint32_t FLIPPED_VERTICALLY_FLAG   = 0x40000000;
    const uint32_t FLIPPED_DIAGONALLY_FLAG   = 0x20000000;
    const uint32_t GID_MASK                  = 0x1FFFFFFF;
    (void)FLIPPED_DIAGONALLY_FLAG;

    for (size_t li = 0; li < map->layer_count; ++li) {
        const tiled_layer_t* layer = &map->layers[li];
        if (!layer->collision) continue;

        size_t idx = 0;
        for (int y = 0; y < layer->height; ++y) {
            for (int x = 0; x < layer->width; ++x, ++idx) {
                if ((size_t)x >= (size_t)map->width || (size_t)y >= (size_t)map->height) continue;
                uint32_t raw_gid = layer->gids[idx];
                if (raw_gid == 0) continue;

                bool flip_h = (raw_gid & FLIPPED_HORIZONTALLY_FLAG) != 0;
                bool flip_v = (raw_gid & FLIPPED_VERTICALLY_FLAG) != 0;
                uint32_t gid = raw_gid & GID_MASK;
                if (gid == 0) continue;

                size_t ts_idx = 0;
                int local = 0;
                const tiled_tileset_t* ts = tileset_for_gid_runtime(map, gid, &ts_idx, &local);
                if (!ts) continue;

                int draw_idx = animated_tile_index_now(ts, local);
                uint16_t mask = (ts->colliders && draw_idx >= 0 && draw_idx < ts->tilecount) ? ts->colliders[draw_idx] : 0;
                if (flip_h) mask = flip_mask_h(mask);
                if (flip_v) mask = flip_mask_v(mask);

                size_t write_idx = (size_t)y * (size_t)map->width + (size_t)x;
                g_world.subtile_masks[write_idx] = mask;
                g_world.tiles[write_idx] = (mask == subtile_full_mask()) ? WORLD_TILE_SOLID : WORLD_TILE_WALKABLE;
            }
        }
    }
}
