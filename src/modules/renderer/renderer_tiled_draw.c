#include "modules/renderer/renderer_internal.h"
#include "modules/asset/asset_renderer_internal.h"
#include "modules/world/world.h"

#include <limits.h>
#include <math.h>
#include <string.h>

static const char* ENTITY_LAYER_NAME = "entities"; // TMX object layer used to spawn ECS entities (not rendered directly)

bool visible_tile_range(const world_map_t* map,
                        Rectangle padded_view,
                        int* out_startX, int* out_startY,
                        int* out_endX, int* out_endY,
                        int* out_visible_tiles)
{
    if (!map || map->tilewidth <= 0 || map->tileheight <= 0) return false;
    if (map->width <= 0 || map->height <= 0) return false;

    int tw = map->tilewidth;
    int th = map->tileheight;
    Rectangle map_rect = {0.0f, 0.0f, (float)(map->width * tw), (float)(map->height * th)};
    Rectangle vis = intersect_rect(map_rect, padded_view);
    if (vis.width <= 0.0f || vis.height <= 0.0f) return false;

    int startX = (int)floorf(vis.x / (float)tw);
    int startY = (int)floorf(vis.y / (float)th);
    int endX   = (int)ceilf((vis.x + vis.width) / (float)tw);
    int endY   = (int)ceilf((vis.y + vis.height) / (float)th);

    if (startX < 0) startX = 0;
    if (startY < 0) startY = 0;
    if (endX > map->width) endX = map->width;
    if (endY > map->height) endY = map->height;

    int visible_tiles = (endX - startX) * (endY - startY);
    if (visible_tiles < 0) visible_tiles = 0;

    if (out_startX) *out_startX = startX;
    if (out_startY) *out_startY = startY;
    if (out_endX) *out_endX = endX;
    if (out_endY) *out_endY = endY;
    if (out_visible_tiles) *out_visible_tiles = visible_tiles;
    return true;
}

typedef struct {
    const tiled_tileset_t* ts;
    size_t ts_idx;
    tex_handle_t tex_handle;
    Texture2D tex_value;
    Rectangle src;
    int local_index;
    int draw_index;
} resolved_gid_t;

static const tiled_tileset_t* tileset_for_gid(const world_map_t* map, uint32_t gid, size_t* out_index)
{
    if (!map || map->tileset_count == 0) return NULL;

    // Binary search for the last tileset with first_gid <= gid.
    size_t lo = 0;
    size_t hi = map->tileset_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        uint32_t first_gid = (uint32_t)map->tilesets[mid].first_gid;
        if (gid < first_gid) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }

    if (lo == 0) return NULL;
    size_t idx = lo - 1;
    const tiled_tileset_t *ts = &map->tilesets[idx];
    int local = (int)gid - ts->first_gid;
    if (local < 0 || local >= ts->tilecount) return NULL;
    if (out_index) *out_index = idx;
    return ts;
}

static int animated_tile_index(const tiled_tileset_t *ts, int base_index, double now_ms)
{
    if (!ts || !ts->anims || base_index < 0 || base_index >= ts->tilecount) return base_index;
    tiled_animation_t *anim = &ts->anims[base_index];
    if (!anim || anim->frame_count == 0 || anim->total_duration_ms <= 0) return base_index;

    double mod = fmod(now_ms, (double)anim->total_duration_ms);
    int acc = 0;
    for (size_t i = 0; i < anim->frame_count; ++i) {
        acc += anim->frames[i].duration_ms;
        if (mod < (double)acc) {
            int idx = anim->frames[i].tile_id;
            if (idx >= 0 && idx < ts->tilecount) return idx;
            break;
        }
    }
    return base_index;
}

static bool resolve_gid_draw(const world_map_t* map,
                             const tiled_renderer_t* tr,
                             uint32_t raw_gid,
                             bool allow_animation,
                             double now_ms,
                             resolved_gid_t* out,
                             bool* out_flip_h,
                             bool* out_flip_v)
{
    if (!map || !tr || !out) return false;
    if (raw_gid == 0) return false;

    bool flip_h = false, flip_v = false, flip_d = false;
    uint32_t gid = tiled_gid_strip_flags(raw_gid, &flip_h, &flip_v, &flip_d);
    if (gid == 0) return false;
    (void)flip_d;

    size_t ts_idx = 0;
    const tiled_tileset_t* ts = tileset_for_gid(map, gid, &ts_idx);
    if (!ts || ts_idx >= tr->texture_count) return false;

    tex_handle_t handle = tr->tilesets[ts_idx];
    Texture2D tex = asset_backend_resolve_texture_value(handle);
    if (tex.id == 0) return false;

    int local = (int)gid - ts->first_gid;
    if (local < 0 || local >= ts->tilecount) return false;

    int draw_index = allow_animation ? animated_tile_index(ts, local, now_ms) : local;
    int columns = ts->columns > 0 ? ts->columns : 1;

    int sx = (draw_index % columns) * ts->tilewidth;
    int sy = (draw_index / columns) * ts->tileheight;
    Rectangle src = { (float)sx, (float)sy, (float)ts->tilewidth, (float)ts->tileheight };
    if (flip_h) {
        src.width = -src.width;
    }
    if (flip_v) {
        src.height = -src.height;
    }

    *out = (resolved_gid_t){
        .ts = ts,
        .ts_idx = ts_idx,
        .tex_handle = handle,
        .tex_value = tex,
        .src = src,
        .local_index = local,
        .draw_index = draw_index,
    };
    if (out_flip_h) *out_flip_h = flip_h;
    if (out_flip_v) *out_flip_v = flip_v;
    return true;
}

static void draw_or_enqueue_resolved(const resolved_gid_t* r,
                                     Rectangle dst,
                                     float painter_key,
                                     bool painter_tile,
                                     painter_queue_ctx_t* painter_ctx)
{
    if (!r) return;
    if (painter_tile && painter_ctx && painter_ctx->queue) {
        int seq = (int)painter_ctx->queue->size;
        ecs_sprite_view_t v = {
            .tex = r->tex_handle,
            .src = rectf_from_rect(r->src),
            .x   = dst.x,
            .y   = dst.y,
            .ox  = 0.0f,
            .oy  = 0.0f
        };
        Item item = { .v = v, .key = painter_key, .seq = seq };
        painter_queue_push(painter_ctx, item);
    } else {
        DrawTexturePro(r->tex_value, r->src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
    }
}

static void draw_tile_layer(const world_map_t* map,
                            const tiled_renderer_t* tr,
                            const tiled_layer_t* layer,
                            int startX, int startY, int endX, int endY,
                            double now_ms,
                            painter_queue_ctx_t* painter_ctx)
{
    if (!map || !tr || !layer) return;
    if (!layer->gids || layer->width <= 0 || layer->height <= 0) return;
    int tw = map->tilewidth;
    int th = map->tileheight;
    if (startX < 0) startX = 0;
    if (startY < 0) startY = 0;
    if (endX > layer->width) endX = layer->width;
    if (endY > layer->height) endY = layer->height;
    if (endX <= startX || endY <= startY) return;

    for (int y = startY; y < endY; ++y) {
        size_t row_start = (size_t)y * (size_t)layer->width;
        for (int x = startX; x < endX; ++x) {
            size_t idx = row_start + (size_t)x;
            uint32_t raw_gid = layer->gids[idx];
            resolved_gid_t r;
            if (!resolve_gid_draw(map, tr, raw_gid, true, now_ms, &r, NULL, NULL)) continue;

            Rectangle dst = { (float)(x * tw), (float)(y * th), (float)tw, (float)th };

            bool painter_tile = (r.ts->render_painters && r.draw_index >= 0 && r.draw_index < r.ts->tilecount)
                ? r.ts->render_painters[r.draw_index]
                : false;
            float painter_off = (r.ts->painter_offset && r.draw_index >= 0 && r.draw_index < r.ts->tilecount)
                ? (float)r.ts->painter_offset[r.draw_index]
                : 0.0f;
            float key = dst.y + painter_off;
            draw_or_enqueue_resolved(&r, dst, key, painter_tile, painter_ctx);
        }
    }
}

static size_t draw_object_layer_at_z(const world_map_t* map,
                                     const tiled_renderer_t* tr,
                                     const render_view_t* view,
                                     painter_queue_ctx_t* painter_ctx,
                                     size_t obj_start,
                                     int target_z,
                                     double now_ms)
{
    if (!map || !tr || !view) return obj_start;

    size_t i = obj_start;
    for (; i < map->object_count; ++i) {
        const tiled_object_t* obj = &map->objects[i];
        if (obj->layer_z != target_z) break;
        if (obj->layer_name && strcmp(obj->layer_name, ENTITY_LAYER_NAME) == 0) continue;
        if (obj->gid == 0) continue;

        resolved_gid_t r;
        if (!resolve_gid_draw(map, tr, (uint32_t)obj->gid, false, now_ms, &r, NULL, NULL)) continue;

        float dst_w = (obj->w > 0.0f) ? obj->w : (float)r.ts->tilewidth;
        float dst_h = (obj->h > 0.0f) ? obj->h : (float)r.ts->tileheight;
        Rectangle dst = { obj->x, obj->y - dst_h, dst_w, dst_h }; // Tiled object y is bottom

        if (!rects_intersect(dst, view->padded_view)) continue;

        bool painter_tile = (r.ts->render_painters && r.local_index >= 0 && r.local_index < r.ts->tilecount)
            ? r.ts->render_painters[r.local_index]
            : false;
        float painter_off = (r.ts->painter_offset && r.local_index >= 0 && r.local_index < r.ts->tilecount)
            ? (float)r.ts->painter_offset[r.local_index]
            : 0.0f;
        float key = dst.y + painter_off;
        draw_or_enqueue_resolved(&r, dst, key, painter_tile, painter_ctx);
    }
    return i;
}

void draw_tmx_stack(const world_map_t* map,
                    const render_view_t* view,
                    int startX, int startY, int endX, int endY,
                    double now_ms,
                    painter_queue_ctx_t* painter_ctx)
{
    if (!view || !painter_ctx) return;

    renderer_ctx_t* ctx = renderer_ctx_get();
    const tiled_renderer_t* tr = &ctx->tiled;

    size_t layer_i = 0;
    size_t obj_i = 0;
    if (!map) return;
    while (layer_i < map->layer_count || obj_i < map->object_count) {
        int next_layer_z = (layer_i < map->layer_count) ? map->layers[layer_i].z_order : INT_MAX;
        int next_obj_z   = (obj_i < map->object_count) ? map->objects[obj_i].layer_z : INT_MAX;
        int z = (next_layer_z < next_obj_z) ? next_layer_z : next_obj_z;

        // Draw all tile layers at this z.
        while (layer_i < map->layer_count && map->layers[layer_i].z_order == z) {
            draw_tile_layer(map, tr, &map->layers[layer_i], startX, startY, endX, endY, now_ms, painter_ctx);
            layer_i++;
        }

        // Draw all objects at this z.
        if (obj_i < map->object_count && map->objects[obj_i].layer_z == z) {
            obj_i = draw_object_layer_at_z(map, tr, view, painter_ctx, obj_i, z, now_ms);
        }
    }
}

void draw_world_fallback_tiles(const render_view_t* view)
{
    int tileSize = world_tile_size();
    int tilesW = 0, tilesH = 0;
    world_size_tiles(&tilesW, &tilesH);

    Rectangle worldRect = {0.0f, 0.0f, (float)(tilesW * tileSize), (float)(tilesH * tileSize)};
    Rectangle visible = intersect_rect(worldRect, view->padded_view);
    if (visible.width <= 0.0f || visible.height <= 0.0f) return;

    int startX = (int)floorf(visible.x / tileSize);
    int startY = (int)floorf(visible.y / tileSize);
    int endX   = (int)ceilf((visible.x + visible.width) / tileSize);
    int endY   = (int)ceilf((visible.y + visible.height) / tileSize);

    if (startX < 0) startX = 0;
    if (startY < 0) startY = 0;
    if (endX > tilesW) endX = tilesW;
    if (endY > tilesH) endY = tilesH;

    for (int ty = startY; ty < endY; ++ty) {
        for (int tx = startX; tx < endX; ++tx) {
            world_tile_t t = world_tile_at(tx, ty);
            switch (t) {
                case WORLD_TILE_WALKABLE: {
                    Color c = ((tx + ty) % 2 == 0) ? LIGHTGRAY : DARKGRAY;
                    DrawRectangle(tx * tileSize, ty * tileSize, tileSize, tileSize, c);
                } break;
                case WORLD_TILE_SOLID: {
                    Color c = BROWN;
                    DrawRectangle(tx * tileSize, ty * tileSize, tileSize, tileSize, c);
                } break;
                default:
                    break;
            }
        }
    }
}
