#include "../includes/world_collision.h"
#include "../includes/world.h"
#include "../includes/logger.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

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
    bool* dynamic_tiles; // per-tile flag (derived from tileset property)
    v2f spawn;         // pixel position at tile center
} world_collision_state_t;

static world_collision_state_t g_world = { .tile_size = WORLD_TILE_SIZE };

static void world_reset_state(world_collision_state_t* ws)
{
    if (!ws) return;
    free(ws->tiles);
    free(ws->subtile_masks);
    free(ws->dynamic_tiles);
    *ws = (world_collision_state_t){ .tile_size = WORLD_TILE_SIZE };
}

int world_tile_size(void) { return WORLD_TILE_SIZE; }
int world_subtile_size(void) { return WORLD_SUBTILE_SIZE; }

static uint16_t subtile_full_mask(void)
{
    return (uint16_t)((1u << WORLD_SUBTILES_PER_TILE_TOTAL) - 1u);
}

static v2f tile_center_px(int tx, int ty, int tile_size)
{
    return v2f_make((tx + 0.5f) * (float)tile_size, (ty + 0.5f) * (float)tile_size);
}

static uint16_t flip_mask_h(uint16_t mask)
{
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

static uint16_t flip_mask_v(uint16_t mask)
{
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

static const tiled_tileset_t* tileset_for_gid_runtime(const tiled_map_t* map, uint32_t gid, int* out_local)
{
    if (!map) return NULL;
    for (size_t i = 0; i < map->tileset_count; ++i) {
        const tiled_tileset_t* ts = &map->tilesets[i];
        int local = (int)gid - ts->first_gid;
        if (local < 0 || local >= ts->tilecount) continue;
        if (out_local) *out_local = local;
        return ts;
    }
    return NULL;
}

bool world_collision_decode_raw_gid(const tiled_map_t* map, uint32_t raw_gid, uint16_t* out_mask, bool* out_dynamic)
{
    if (out_mask) *out_mask = 0;
    if (out_dynamic) *out_dynamic = false;
    if (!map || raw_gid == 0) return false;

    bool flip_h = false, flip_v = false, flip_d = false;
    uint32_t gid = tiled_gid_strip_flags(raw_gid, &flip_h, &flip_v, &flip_d);
    if (gid == 0) return false;
    (void)flip_d;

    int local = 0;
    const tiled_tileset_t* ts = tileset_for_gid_runtime(map, gid, &local);
    if (!ts) return false;

    uint16_t mask = (ts->colliders && local >= 0 && local < ts->tilecount) ? ts->colliders[local] : 0;
    if (flip_h) mask = flip_mask_h(mask);
    if (flip_v) mask = flip_mask_v(mask);

    bool dynamic = false;
    if (ts->no_merge_collider && local >= 0 && local < ts->tilecount) {
        dynamic = ts->no_merge_collider[local];
    }

    if (out_mask) *out_mask = mask;
    if (out_dynamic) *out_dynamic = dynamic;
    return true;
}

static bool is_collision_layer(tiled_layer_t* layer, const char* collision_layer_name)
{
    if (!layer) return false;
    if (layer->collision) return true;
    if (collision_layer_name && layer->name && strcmp(layer->name, collision_layer_name) == 0) {
        layer->collision = true; // persist so runtime edits know this layer affects collision
        return true;
    }
    return false;
}

bool world_collision_build_from_map(tiled_map_t* map, const char* collision_layer_name)
{
    if (!map) return false;
    if (map->tilewidth != WORLD_TILE_SIZE || map->tileheight != WORLD_TILE_SIZE) {
        LOGC(LOGCAT_MAIN, LOG_LVL_WARN, "world: TMX tile size %dx%d differs from engine tile size %d", map->tilewidth, map->tileheight, WORLD_TILE_SIZE);
    }

    const size_t count = (size_t)map->width * (size_t)map->height;
    world_tile_t* tiles = (world_tile_t*)malloc(count * sizeof(world_tile_t));
    uint16_t* masks = (uint16_t*)malloc(count * sizeof(uint16_t));
    bool* dynamic = (bool*)malloc(count * sizeof(bool));
    if (!tiles || !masks || !dynamic) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "world: out of memory for collision (%d x %d)", map->width, map->height);
        free(tiles);
        free(masks);
        free(dynamic);
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        tiles[i] = WORLD_TILE_WALKABLE;
        masks[i] = 0;
        dynamic[i] = false;
    }

    for (int y = 0; y < map->height; ++y) {
        for (int x = 0; x < map->width; ++x) {
            uint32_t raw_gid = 0;
            // Topmost collision layer wins.
            for (size_t li = map->layer_count; li-- > 0; ) {
                tiled_layer_t* layer = &map->layers[li];
                if (!is_collision_layer(layer, collision_layer_name)) continue;
                if (x < 0 || y < 0 || x >= layer->width || y >= layer->height) continue;
                if (!layer->gids) continue;
                uint32_t gid = layer->gids[(size_t)y * (size_t)layer->width + (size_t)x];
                if (gid == 0) continue;
                raw_gid = gid;
                break;
            }

            uint16_t mask = 0;
            bool dyn = false;
            if (raw_gid != 0) {
                world_collision_decode_raw_gid(map, raw_gid, &mask, &dyn);
            }

            const size_t idx = (size_t)y * (size_t)map->width + (size_t)x;
            masks[idx] = mask;
            tiles[idx] = (mask == subtile_full_mask()) ? WORLD_TILE_SOLID : WORLD_TILE_WALKABLE;
            dynamic[idx] = dyn;
        }
    }

    world_reset_state(&g_world);
    g_world = (world_collision_state_t){
        .w = map->width,
        .h = map->height,
        .tile_size = WORLD_TILE_SIZE,
        .tiles = tiles,
        .subtile_masks = masks,
        .dynamic_tiles = dynamic,
        .spawn = tile_center_px(map->width / 2, map->height / 2, WORLD_TILE_SIZE),
    };

    return true;
}

void world_collision_shutdown(void)
{
    world_reset_state(&g_world);
}

void world_collision_refresh_tile(const tiled_map_t* map, int tx, int ty)
{
    if (!map) return;
    if (!g_world.tiles || !g_world.subtile_masks) return;
    if (tx < 0 || ty < 0 || tx >= map->width || ty >= map->height) return;

    uint32_t raw_gid = 0;
    for (size_t li = map->layer_count; li-- > 0; ) {
        const tiled_layer_t* layer = &map->layers[li];
        if (!layer->collision) continue;
        if (tx >= layer->width || ty >= layer->height) continue;
        if (!layer->gids) continue;
        uint32_t gid = layer->gids[(size_t)ty * (size_t)layer->width + (size_t)tx];
        if (gid == 0) continue;
        raw_gid = gid;
        break;
    }

    uint16_t mask = 0;
    bool dyn = false;
    if (raw_gid != 0) {
        world_collision_decode_raw_gid(map, raw_gid, &mask, &dyn);
    }

    const size_t idx = (size_t)ty * (size_t)map->width + (size_t)tx;
    g_world.subtile_masks[idx] = mask;
    g_world.tiles[idx] = (mask == subtile_full_mask()) ? WORLD_TILE_SOLID : WORLD_TILE_WALKABLE;
    if (g_world.dynamic_tiles) g_world.dynamic_tiles[idx] = dyn;
}

// ===== Existing public query API (world.h) =====
void world_size_tiles(int* out_w, int* out_h)
{
    if (out_w) *out_w = g_world.w;
    if (out_h) *out_h = g_world.h;
}

void world_size_px(int* out_w, int* out_h)
{
    int ts = g_world.tile_size;
    if (out_w) *out_w = g_world.w * ts;
    if (out_h) *out_h = g_world.h * ts;
}

world_tile_t world_tile_at(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= g_world.w || ty >= g_world.h || !g_world.tiles) {
        return WORLD_TILE_VOID;
    }
    return g_world.tiles[ty * g_world.w + tx];
}

uint16_t world_subtile_mask_at(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= g_world.w || ty >= g_world.h || !g_world.subtile_masks) {
        return 0;
    }
    return g_world.subtile_masks[(size_t)ty * (size_t)g_world.w + (size_t)tx];
}

bool world_tile_is_dynamic(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= g_world.w || ty >= g_world.h || !g_world.dynamic_tiles) {
        return false;
    }
    return g_world.dynamic_tiles[(size_t)ty * (size_t)g_world.w + (size_t)tx];
}

bool world_is_walkable_subtile(int sx, int sy)
{
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

bool world_is_walkable_px(float x, float y)
{
    int ss = world_subtile_size();
    if (ss <= 0 || g_world.w <= 0 || g_world.h <= 0) return false;

    int sx = (int)floorf(x / (float)ss);
    int sy = (int)floorf(y / (float)ss);
    return world_is_walkable_subtile(sx, sy);
}

bool world_is_walkable_rect_px(float cx, float cy, float hx, float hy)
{
    int ss = world_subtile_size();
    if (ss <= 0 || g_world.w <= 0 || g_world.h <= 0) return false;

    float left   = cx - hx;
    float right  = cx + hx;
    float bottom = cy - hy;
    float top    = cy + hy;

    int sx0 = (int)floorf(left / (float)ss);
    int sx1 = (int)floorf(right / (float)ss);
    int sy0 = (int)floorf(bottom / (float)ss);
    int sy1 = (int)floorf(top / (float)ss);

    for (int sy = sy0; sy <= sy1; ++sy) {
        for (int sx = sx0; sx <= sx1; ++sx) {
            if (!world_is_walkable_subtile(sx, sy)) return false;
        }
    }
    return true;
}

bool world_has_line_of_sight(float x0, float y0, float x1, float y1, float max_range, float hx, float hy)
{
    int ss = world_subtile_size();
    if (ss <= 0 || g_world.w <= 0 || g_world.h <= 0) return false;

    float dx = x1 - x0;
    float dy = y1 - y0;
    float dist2 = dx * dx + dy * dy;
    if (dist2 <= 0.0f) return true;
    if (max_range > 0.0f && dist2 > max_range * max_range) return false;

    float dist = sqrtf(dist2);
    float step = (float)ss * 0.5f;
    if (step <= 0.0f) step = 4.0f;
    int steps = (int)ceilf(dist / step);
    if (steps < 1) steps = 1;
    float inv_steps = 1.0f / (float)steps;

    for (int i = 0; i <= steps; ++i) {
        float t = (float)i * inv_steps;
        float px = x0 + dx * t;
        float py = y0 + dy * t;
        if (!world_is_walkable_rect_px(px, py, hx, hy)) return false;
    }
    return true;
}

v2f world_get_spawn_px(void)
{
    if (!g_world.tiles) {
        return v2f_make(0.0f, 0.0f);
    }
    return v2f_make(g_world.spawn.x, g_world.spawn.y);
}

