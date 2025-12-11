#include "tiled_renderer.h"
#include <stdio.h>

bool tiled_renderer_init(tiled_renderer_t *r, const tiled_map_t *map) {
    if (!r || !map) return false;
    *r = (tiled_renderer_t){0};
    if (!map->tileset.image_path) {
        fprintf(stderr, "Tileset image path missing\n");
        return false;
    }
    r->tileset = LoadTexture(map->tileset.image_path);
    if (r->tileset.id == 0) {
        fprintf(stderr, "Failed to load tileset texture: %s\n", map->tileset.image_path);
        return false;
    }
    return true;
}

void tiled_renderer_shutdown(tiled_renderer_t *r) {
    if (!r) return;
    if (r->tileset.id != 0) {
        UnloadTexture(r->tileset);
    }
    *r = (tiled_renderer_t){0};
}

void tiled_renderer_draw(const tiled_map_t *map, const tiled_renderer_t *r) {
    if (!map || !r || r->tileset.id == 0) return;
    int tw = map->tilewidth;
    int th = map->tileheight;
    int columns = map->tileset.columns > 0 ? map->tileset.columns : 1;
    int first_gid = map->tileset.first_gid;
    int total = map->tileset.tilecount;

    const uint32_t FLIPPED_HORIZONTALLY_FLAG = 0x80000000;
    const uint32_t FLIPPED_VERTICALLY_FLAG   = 0x40000000;
    const uint32_t FLIPPED_DIAGONALLY_FLAG   = 0x20000000;
    const uint32_t GID_MASK                  = 0x1FFFFFFF;

    for (size_t l = 0; l < map->layer_count; ++l) {
        const tiled_layer_t *layer = &map->layers[l];
        size_t idx = 0;
        for (int y = 0; y < layer->height; ++y) {
            for (int x = 0; x < layer->width; ++x, ++idx) {
                uint32_t raw_gid = layer->gids[idx];
                if (raw_gid == 0) continue;

                bool flip_h = (raw_gid & FLIPPED_HORIZONTALLY_FLAG) != 0;
                bool flip_v = (raw_gid & FLIPPED_VERTICALLY_FLAG) != 0;
                bool flip_d = (raw_gid & FLIPPED_DIAGONALLY_FLAG) != 0;
                (void)flip_d; // Diagonal/rotation not handled yet.

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
                DrawTexturePro(r->tileset, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
            }
        }
    }
}
