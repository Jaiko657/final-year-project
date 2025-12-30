#include "modules/world/world_collision_internal.h"
#include "modules/world/world.h"
#include "modules/core/logger.h"

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
} world_collision_grid_t;

static world_collision_grid_t g_collision = { .tile_size = WORLD_TILE_SIZE };

static void collision_grid_reset(world_collision_grid_t* grid)
{
    if (!grid) return;
    free(grid->tiles);
    free(grid->subtile_masks);
    free(grid->dynamic_tiles);
    *grid = (world_collision_grid_t){ .tile_size = WORLD_TILE_SIZE };
}

int world_tile_size(void) { return WORLD_TILE_SIZE; }
int world_subtile_size(void) { return WORLD_SUBTILE_SIZE; }

static uint16_t subtile_full_mask(void)
{
    return (uint16_t)((1u << WORLD_SUBTILES_PER_TILE_TOTAL) - 1u);
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

static const tiled_tileset_t* tileset_for_gid_runtime(const world_map_t* map, uint32_t gid, int* out_local)
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

bool world_collision_decode_raw_gid(const world_map_t* map, uint32_t raw_gid, uint16_t* out_mask, bool* out_dynamic)
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

static bool layer_is_collision(tiled_layer_t* layer, const char* collision_layer_name, bool allow_mark)
{
    if (!layer) return false;
    if (layer->collision) return true;
    if (!collision_layer_name || !layer->name) return false;
    if (strcmp(layer->name, collision_layer_name) != 0) return false;
    if (allow_mark) layer->collision = true;
    return true;
}

static uint32_t collision_raw_gid_build(world_map_t* map, int tx, int ty, const char* collision_layer_name)
{
    for (size_t li = map->layer_count; li-- > 0; ) {
        tiled_layer_t* layer = &map->layers[li];
        if (!layer_is_collision(layer, collision_layer_name, true)) continue;
        if (tx >= layer->width || ty >= layer->height) continue;
        if (!layer->gids) continue;
        uint32_t gid = layer->gids[(size_t)ty * (size_t)layer->width + (size_t)tx];
        if (gid == 0) continue;
        return gid;
    }
    return 0;
}

static uint32_t collision_raw_gid_runtime(const world_map_t* map, int tx, int ty)
{
    for (size_t li = map->layer_count; li-- > 0; ) {
        const tiled_layer_t* layer = &map->layers[li];
        if (!layer->collision) continue;
        if (tx >= layer->width || ty >= layer->height) continue;
        if (!layer->gids) continue;
        uint32_t gid = layer->gids[(size_t)ty * (size_t)layer->width + (size_t)tx];
        if (gid == 0) continue;
        return gid;
    }
    return 0;
}

static void collision_grid_write_cell(world_collision_grid_t* grid, const world_map_t* map, int tx, int ty, uint32_t raw_gid)
{
    uint16_t mask = 0;
    bool dyn = false;
    if (raw_gid != 0) {
        world_collision_decode_raw_gid(map, raw_gid, &mask, &dyn);
    }

    const size_t idx = (size_t)ty * (size_t)map->width + (size_t)tx;
    grid->subtile_masks[idx] = mask;
    grid->tiles[idx] = (mask == subtile_full_mask()) ? WORLD_TILE_SOLID : WORLD_TILE_WALKABLE;
    if (grid->dynamic_tiles) grid->dynamic_tiles[idx] = dyn;
}

bool world_collision_build_from_map(world_map_t* map, const char* collision_layer_name)
{
    if (!map) return false;
    if (map->tilewidth != WORLD_TILE_SIZE || map->tileheight != WORLD_TILE_SIZE) {
        LOGC(LOGCAT_WORLD, LOG_LVL_WARN, "world: TMX tile size %dx%d differs from engine tile size %d", map->tilewidth, map->tileheight, WORLD_TILE_SIZE);
    }

    const size_t count = (size_t)map->width * (size_t)map->height;
    world_tile_t* tiles = (world_tile_t*)malloc(count * sizeof(world_tile_t));
    uint16_t* masks = (uint16_t*)malloc(count * sizeof(uint16_t));
    bool* dynamic = (bool*)malloc(count * sizeof(bool));
    if (!tiles || !masks || !dynamic) {
        LOGC(LOGCAT_WORLD, LOG_LVL_ERROR, "world: out of memory for collision (%d x %d)", map->width, map->height);
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
            uint32_t raw_gid = collision_raw_gid_build(map, x, y, collision_layer_name);
            const size_t idx = (size_t)y * (size_t)map->width + (size_t)x;
            uint16_t mask = 0;
            bool dyn = false;
            if (raw_gid != 0) {
                world_collision_decode_raw_gid(map, raw_gid, &mask, &dyn);
            }
            masks[idx] = mask;
            tiles[idx] = (mask == subtile_full_mask()) ? WORLD_TILE_SOLID : WORLD_TILE_WALKABLE;
            dynamic[idx] = dyn;
        }
    }

    collision_grid_reset(&g_collision);
    g_collision = (world_collision_grid_t){
        .w = map->width,
        .h = map->height,
        .tile_size = WORLD_TILE_SIZE,
        .tiles = tiles,
        .subtile_masks = masks,
        .dynamic_tiles = dynamic,
    };

    return true;
}

void world_collision_shutdown(void)
{
    collision_grid_reset(&g_collision);
}

void world_collision_refresh_tile(const world_map_t* map, int tx, int ty)
{
    if (!map) return;
    if (!g_collision.tiles || !g_collision.subtile_masks) return;
    if (tx < 0 || ty < 0 || tx >= map->width || ty >= map->height) return;

    uint32_t raw_gid = collision_raw_gid_runtime(map, tx, ty);
    collision_grid_write_cell(&g_collision, map, tx, ty, raw_gid);
}

void world_size_tiles(int* out_w, int* out_h)
{
    if (out_w) *out_w = g_collision.w;
    if (out_h) *out_h = g_collision.h;
}

void world_size_px(int* out_w, int* out_h)
{
    int ts = g_collision.tile_size;
    if (out_w) *out_w = g_collision.w * ts;
    if (out_h) *out_h = g_collision.h * ts;
}

world_tile_t world_tile_at(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= g_collision.w || ty >= g_collision.h || !g_collision.tiles) {
        return WORLD_TILE_VOID;
    }
    return g_collision.tiles[ty * g_collision.w + tx];
}

uint16_t world_subtile_mask_at(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= g_collision.w || ty >= g_collision.h || !g_collision.subtile_masks) {
        return 0;
    }
    return g_collision.subtile_masks[(size_t)ty * (size_t)g_collision.w + (size_t)tx];
}

bool world_tile_is_dynamic(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= g_collision.w || ty >= g_collision.h || !g_collision.dynamic_tiles) {
        return false;
    }
    return g_collision.dynamic_tiles[(size_t)ty * (size_t)g_collision.w + (size_t)tx];
}

bool world_is_walkable_subtile(int sx, int sy)
{
    if (sx < 0 || sy < 0) return false;
    if (!g_collision.tiles || !g_collision.subtile_masks) return false;
    const int subtiles_per_tile = WORLD_SUBTILES_PER_TILE;
    if (subtiles_per_tile <= 0 || g_collision.w <= 0 || g_collision.h <= 0) return false;

    int tile_x = sx / subtiles_per_tile;
    int tile_y = sy / subtiles_per_tile;
    if (tile_x < 0 || tile_y < 0 || tile_x >= g_collision.w || tile_y >= g_collision.h) return false;

    int local_x = sx % subtiles_per_tile;
    int local_y = sy % subtiles_per_tile;
    int bit = local_y * subtiles_per_tile + local_x;

    size_t idx = (size_t)tile_y * (size_t)g_collision.w + (size_t)tile_x;
    uint16_t mask = g_collision.subtile_masks ? g_collision.subtile_masks[idx] : 0;
    return (mask & (uint16_t)(1u << bit)) == 0;
}

bool world_is_walkable_px(float x, float y)
{
    int ss = world_subtile_size();
    if (ss <= 0 || g_collision.w <= 0 || g_collision.h <= 0) return false;

    int sx = (int)floorf(x / (float)ss);
    int sy = (int)floorf(y / (float)ss);
    return world_is_walkable_subtile(sx, sy);
}

bool world_is_walkable_rect_px(float cx, float cy, float hx, float hy)
{
    int ss = world_subtile_size();
    if (ss <= 0 || g_collision.w <= 0 || g_collision.h <= 0) return false;

    float left   = cx - hx;
    float right  = cx + hx;
    float bottom = cy - hy;
    float top    = cy + hy;

    // Treat right/top edges as exclusive so an AABB exactly aligned to a subtile boundary
    // does not query the adjacent subtile as "occupied".
    if (hx > 0.0f) right = nextafterf(right, -INFINITY);
    if (hy > 0.0f) top   = nextafterf(top, -INFINITY);

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

bool world_resolve_rect_axis_px(float* io_cx, float* io_cy, float hx, float hy, bool axis_x)
{
    if (!io_cx || !io_cy) return false;

    int tiles_w = 0, tiles_h = 0;
    world_size_tiles(&tiles_w, &tiles_h);
    int tile_px = world_tile_size();
    int subtile_px = world_subtile_size();
    if (tiles_w <= 0 || tiles_h <= 0 || tile_px <= 0 || subtile_px <= 0) return false;
    int subtiles_per_tile = tile_px / subtile_px;
    if (subtiles_per_tile <= 0) return false;
    int subtiles_w = tiles_w * subtiles_per_tile;
    int subtiles_h = tiles_h * subtiles_per_tile;
    if (subtiles_w <= 0 || subtiles_h <= 0) return false;

    if (hx <= 0.0f || hy <= 0.0f) return false;

    float cx = *io_cx;
    float cy = *io_cy;

    bool moved_any = false;
    for (int pass = 0; pass < 4; ++pass) {
        float left = cx - hx;
        float right = cx + hx;
        float bottom = cy - hy;
        float top = cy + hy;

        int min_sx = (int)floorf(left / (float)subtile_px);
        int max_sx = (int)floorf(right / (float)subtile_px);
        int min_sy = (int)floorf(bottom / (float)subtile_px);
        int max_sy = (int)floorf(top / (float)subtile_px);

        if (min_sx < 0) min_sx = 0;
        if (min_sy < 0) min_sy = 0;
        if (max_sx >= subtiles_w) max_sx = subtiles_w - 1;
        if (max_sy >= subtiles_h) max_sy = subtiles_h - 1;

        bool moved = false;
        bool found_resolve = false;
        float best_resolve = 0.0f;
        for (int sy = min_sy; sy <= max_sy; ++sy) {
            for (int sx = min_sx; sx <= max_sx; ++sx) {
                if (world_is_walkable_subtile(sx, sy)) continue;

                float tile_left = (float)sx * (float)subtile_px;
                float tile_right = tile_left + (float)subtile_px;
                float tile_bottom = (float)sy * (float)subtile_px;
                float tile_top = tile_bottom + (float)subtile_px;

                if (right <= tile_left || left >= tile_right) continue;
                if (top <= tile_bottom || bottom >= tile_top) continue;

                if (axis_x) {
                    float pen_x_left = right - tile_left;
                    float pen_x_right = tile_right - left;
                    float resolve_x = (pen_x_left < pen_x_right) ? -pen_x_left : pen_x_right;
                    if (!found_resolve || fabsf(resolve_x) > fabsf(best_resolve)) {
                        best_resolve = resolve_x;
                        found_resolve = true;
                    }
                } else {
                    float pen_y_bottom = top - tile_bottom;
                    float pen_y_top = tile_top - bottom;
                    float resolve_y = (pen_y_bottom < pen_y_top) ? -pen_y_bottom : pen_y_top;
                    if (!found_resolve || fabsf(resolve_y) > fabsf(best_resolve)) {
                        best_resolve = resolve_y;
                        found_resolve = true;
                    }
                }
            }
        }

        if (found_resolve) {
            if (axis_x) cx += best_resolve;
            else cy += best_resolve;
            moved = true;
        }

        moved_any |= moved;
        if (!moved) break;
    }

    *io_cx = cx;
    *io_cy = cy;
    return moved_any;
}

bool world_resolve_rect_mtv_px(float* io_cx, float* io_cy, float hx, float hy)
{
    if (!io_cx || !io_cy) return false;

    int tiles_w = 0, tiles_h = 0;
    world_size_tiles(&tiles_w, &tiles_h);
    int tile_px = world_tile_size();
    int subtile_px = world_subtile_size();
    if (tiles_w <= 0 || tiles_h <= 0 || tile_px <= 0 || subtile_px <= 0) return false;
    int subtiles_per_tile = tile_px / subtile_px;
    if (subtiles_per_tile <= 0) return false;
    int subtiles_w = tiles_w * subtiles_per_tile;
    int subtiles_h = tiles_h * subtiles_per_tile;
    if (subtiles_w <= 0 || subtiles_h <= 0) return false;

    if (hx <= 0.0f || hy <= 0.0f) return false;

    float cx = *io_cx;
    float cy = *io_cy;

    bool moved_any = false;
    for (int pass = 0; pass < 8; ++pass) {
        float left = cx - hx;
        float right = cx + hx;
        float bottom = cy - hy;
        float top = cy + hy;

        int min_sx = (int)floorf(left / (float)subtile_px);
        int max_sx = (int)floorf(right / (float)subtile_px);
        int min_sy = (int)floorf(bottom / (float)subtile_px);
        int max_sy = (int)floorf(top / (float)subtile_px);

        if (min_sx < 0) min_sx = 0;
        if (min_sy < 0) min_sy = 0;
        if (max_sx >= subtiles_w) max_sx = subtiles_w - 1;
        if (max_sy >= subtiles_h) max_sy = subtiles_h - 1;

        bool found = false;
        bool axis_x = false;
        float best_resolve = 0.0f;

        for (int sy = min_sy; sy <= max_sy; ++sy) {
            for (int sx = min_sx; sx <= max_sx; ++sx) {
                if (world_is_walkable_subtile(sx, sy)) continue;

                float tile_left = (float)sx * (float)subtile_px;
                float tile_right = tile_left + (float)subtile_px;
                float tile_bottom = (float)sy * (float)subtile_px;
                float tile_top = tile_bottom + (float)subtile_px;

                if (right <= tile_left || left >= tile_right) continue;
                if (top <= tile_bottom || bottom >= tile_top) continue;

                float pen_x_left = right - tile_left;
                float pen_x_right = tile_right - left;
                float pen_y_bottom = top - tile_bottom;
                float pen_y_top = tile_top - bottom;

                float resolve_x = (pen_x_left < pen_x_right) ? -pen_x_left : pen_x_right;
                float resolve_y = (pen_y_bottom < pen_y_top) ? -pen_y_bottom : pen_y_top;

                bool use_x = fabsf(resolve_x) < fabsf(resolve_y);
                float candidate = use_x ? resolve_x : resolve_y;

                if (!found || fabsf(candidate) > fabsf(best_resolve)) {
                    best_resolve = candidate;
                    axis_x = use_x;
                    found = true;
                }
            }
        }

        if (!found) break;

        if (axis_x) cx += best_resolve;
        else cy += best_resolve;
        moved_any = true;
    }

    *io_cx = cx;
    *io_cy = cy;
    return moved_any;
}

bool world_resolve_rect_slide_px(float* io_cx, float* io_cy, float hx, float hy)
{
    bool moved_x = world_resolve_rect_axis_px(io_cx, io_cy, hx, hy, true);
    bool moved_y = world_resolve_rect_axis_px(io_cx, io_cy, hx, hy, false);
    return moved_x || moved_y;
}

bool world_has_line_of_sight(float x0, float y0, float x1, float y1, float max_range, float hx, float hy)
{
    int ss = world_subtile_size();
    if (ss <= 0 || g_collision.w <= 0 || g_collision.h <= 0) return false;

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
