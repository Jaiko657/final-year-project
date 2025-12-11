#include "../includes/tiled.h"
#include "../includes/asset_renderer_internal.h"

#include <math.h>

bool tiled_renderer_init(tiled_renderer_t *r, const tiled_map_t *map) {
    if (!r || !map) return false;
    *r = (tiled_renderer_t){0};
    if (!map->tileset.image_path) {
        return false;
    }
    r->tileset = asset_acquire_texture(map->tileset.image_path);
    if (!asset_texture_valid(r->tileset)) {
        r->tileset = (tex_handle_t){0};
        return false;
    }
    return true;
}

void tiled_renderer_shutdown(tiled_renderer_t *r) {
    if (!r) return;
    if (asset_texture_valid(r->tileset)) {
        asset_release_texture(r->tileset);
    }
    *r = (tiled_renderer_t){0};
}

void tiled_renderer_draw(const tiled_map_t *map, const tiled_renderer_t *r, const Rectangle *view) {
    if (!map || !r) return;
    if (!asset_texture_valid(r->tileset)) return;
    Texture2D tex = asset_backend_resolve_texture_value(r->tileset);
    if (tex.id == 0) return;

    int tw = map->tilewidth;
    int th = map->tileheight;
    int columns = map->tileset.columns > 0 ? map->tileset.columns : 1;
    int first_gid = map->tileset.first_gid;
    int total = map->tileset.tilecount;

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

                int tile_index = (int)gid - first_gid;
                if (tile_index < 0 || tile_index >= total) continue;

                int sx = (tile_index % columns) * tw;
                int sy = (tile_index / columns) * th;
                Rectangle src = { (float)sx, (float)sy, (float)tw, (float)th };
                if (flip_h) {
                    src.x += tw;
                    src.width = -src.width;
                }
                if (flip_v) {
                    src.y += th;
                    src.height = -src.height;
                }
                Rectangle dst = { (float)(x * tw), (float)(y * th), (float)tw, (float)th };
                DrawTexturePro(tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
            }
        }
    }
}
