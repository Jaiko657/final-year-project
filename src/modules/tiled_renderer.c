#include "../includes/tiled.h"
#include "../includes/asset_renderer_internal.h"

#include <math.h>
#include <stdlib.h>

static const tiled_tileset_t *tileset_for_gid(const tiled_map_t *map, uint32_t gid, size_t *out_index) {
    const tiled_tileset_t *match = NULL;
    size_t mi = 0;
    for (size_t i = 0; i < map->tileset_count; ++i) {
        const tiled_tileset_t *ts = &map->tilesets[i];
        int local = (int)gid - ts->first_gid;
        if (local < 0 || local >= ts->tilecount) continue;
        match = ts;
        mi = i;
    }
    if (match && out_index) *out_index = mi;
    return match;
}

static int animated_tile_index(const tiled_tileset_t *ts, int base_index) {
    if (!ts || !ts->anims || base_index < 0 || base_index >= ts->tilecount) return base_index;
    tiled_animation_t *anim = &ts->anims[base_index];
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

bool tiled_renderer_init(tiled_renderer_t *r, const tiled_map_t *map) {
    if (!r || !map || map->tileset_count == 0) return false;
    *r = (tiled_renderer_t){0};
    r->texture_count = map->tileset_count;
    r->tilesets = (tex_handle_t *)calloc(r->texture_count, sizeof(tex_handle_t));
    if (!r->tilesets) return false;

    for (size_t i = 0; i < map->tileset_count; ++i) {
        const tiled_tileset_t *ts = &map->tilesets[i];
        if (!ts->image_path) {
            tiled_renderer_shutdown(r);
            return false;
        }
        r->tilesets[i] = asset_acquire_texture(ts->image_path);
        if (!asset_texture_valid(r->tilesets[i])) {
            tiled_renderer_shutdown(r);
            return false;
        }
    }
    return true;
}

void tiled_renderer_shutdown(tiled_renderer_t *r) {
    if (!r) return;
    if (r->tilesets) {
        for (size_t i = 0; i < r->texture_count; ++i) {
            if (asset_texture_valid(r->tilesets[i])) {
                asset_release_texture(r->tilesets[i]);
            }
        }
    }
    free(r->tilesets);
    *r = (tiled_renderer_t){0};
}

void tiled_renderer_draw(const tiled_map_t *map, const tiled_renderer_t *r, const Rectangle *view, tiled_painter_emit_fn emit_painter, void *emit_ud) {
    if (!map || !r || !map->tilesets || !r->tilesets || r->texture_count == 0) return;

    int tw = map->tilewidth;
    int th = map->tileheight;

    const uint32_t FLIPPED_HORIZONTALLY_FLAG = 0x80000000;
    const uint32_t FLIPPED_VERTICALLY_FLAG   = 0x40000000;
    const uint32_t FLIPPED_DIAGONALLY_FLAG   = 0x20000000;
    const uint32_t GID_MASK                  = 0x1FFFFFFF;

    int startX = 0, startY = 0, endX = map->width, endY = map->height;
    if (view) {
        startX = (int)floorf(view->x / (float)tw);
        startY = (int)floorf(view->y / (float)th);
        endX   = (int)ceilf((view->x + view->width) / (float)tw);
        endY   = (int)ceilf((view->y + view->height) / (float)th);
        if (startX < 0) startX = 0;
        if (startY < 0) startY = 0;
        if (endX > map->width) endX = map->width;
        if (endY > map->height) endY = map->height;
    }

    for (size_t l = 0; l < map->layer_count; ++l) {
        const tiled_layer_t *layer = &map->layers[l];
        for (int y = startY; y < endY; ++y) {
            size_t row_start = (size_t)y * (size_t)layer->width;
            for (int x = startX; x < endX; ++x) {
                size_t idx = row_start + (size_t)x;
                uint32_t raw_gid = layer->gids[idx];
                if (raw_gid == 0) continue;

                bool flip_h = (raw_gid & FLIPPED_HORIZONTALLY_FLAG) != 0;
                bool flip_v = (raw_gid & FLIPPED_VERTICALLY_FLAG) != 0;
                bool flip_d = (raw_gid & FLIPPED_DIAGONALLY_FLAG) != 0;
                (void)flip_d;

                uint32_t gid = raw_gid & GID_MASK;
                if (gid == 0) continue;

                size_t ts_idx = 0;
                const tiled_tileset_t *ts = tileset_for_gid(map, gid, &ts_idx);
                if (!ts || ts_idx >= r->texture_count) continue;

                Texture2D tex = asset_backend_resolve_texture_value(r->tilesets[ts_idx]);
                if (tex.id == 0) continue;

                int local = (int)gid - ts->first_gid;
                if (local < 0 || local >= ts->tilecount) continue;
                int draw_index = animated_tile_index(ts, local);
                int columns = ts->columns > 0 ? ts->columns : 1;

                int sx = (draw_index % columns) * ts->tilewidth;
                int sy = (draw_index / columns) * ts->tileheight;
                Rectangle src = { (float)sx, (float)sy, (float)ts->tilewidth, (float)ts->tileheight };
                if (flip_h) {
                    src.x += ts->tilewidth;
                    src.width = -src.width;
                }
                if (flip_v) {
                    src.y += ts->tileheight;
                    src.height = -src.height;
                }
                Rectangle dst = { (float)(x * tw), (float)(y * th), (float)tw, (float)th };

                bool painter_tile = (ts->render_painters && draw_index >= 0 && draw_index < ts->tilecount) ? ts->render_painters[draw_index] : false;
                if (painter_tile && emit_painter) {
                    tiled_painter_tile_t pt = {
                        .tex = r->tilesets[ts_idx],
                        .src = src,
                        .dst = dst,
                        .painter_offset = (ts->painter_offset && draw_index >= 0 && draw_index < ts->tilecount) ? (float)ts->painter_offset[draw_index] : 0.0f
                    };
                    emit_painter(&pt, emit_ud);
                    continue;
                }
                DrawTexturePro(tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
            }
        }
    }
}
