#include "tiled_renderer.h"
#include <stdio.h>
#include <stdlib.h>

static const tiled_tileset_t *tileset_for_gid(const tiled_map_t *map, uint32_t gid, size_t *out_index) {
    const tiled_tileset_t *match = NULL;
    size_t match_idx = 0;
    for (size_t i = 0; i < map->tileset_count; ++i) {
        const tiled_tileset_t *ts = &map->tilesets[i];
        int local = (int)gid - ts->first_gid;
        if (local < 0 || local >= ts->tilecount) continue;
        match = ts;
        match_idx = i;
    }
    if (match && out_index) *out_index = match_idx;
    return match;
}

static int animated_tile_index(const tiled_tileset_t *ts, int base_index) {
    if (!ts || !ts->anims || base_index < 0 || base_index >= ts->tilecount) return base_index;
    tiled_animation_t *anim = &ts->anims[base_index];
    if (!anim || anim->frame_count == 0 || anim->total_duration_ms <= 0) return base_index;

    long long now_ms = (long long)(GetTime() * 1000.0);
    int mod = (int)(now_ms % anim->total_duration_ms);
    int acc = 0;
    for (size_t i = 0; i < anim->frame_count; ++i) {
        acc += anim->frames[i].duration_ms;
        if (mod < acc) {
            int idx = anim->frames[i].tile_id;
            if (idx >= 0 && idx < ts->tilecount) {
                return idx;
            }
            break;
        }
    }
    return base_index;
}

bool tiled_renderer_init(tiled_renderer_t *r, const tiled_map_t *map) {
    if (!r || !map || map->tileset_count == 0 || !map->tilesets) return false;
    *r = (tiled_renderer_t){0};

    r->texture_count = map->tileset_count;
    r->tileset_textures = (Texture2D *)calloc(r->texture_count, sizeof(Texture2D));
    if (!r->tileset_textures) return false;

    for (size_t i = 0; i < map->tileset_count; ++i) {
        const tiled_tileset_t *ts = &map->tilesets[i];
        if (!ts->image_path) {
            fprintf(stderr, "Tileset image path missing for tileset %zu\n", i);
            tiled_renderer_shutdown(r);
            return false;
        }
        r->tileset_textures[i] = LoadTexture(ts->image_path);
        if (r->tileset_textures[i].id == 0) {
            fprintf(stderr, "Failed to load tileset texture: %s\n", ts->image_path);
            tiled_renderer_shutdown(r);
            return false;
        }
    }
    return true;
}

void tiled_renderer_shutdown(tiled_renderer_t *r) {
    if (!r) return;
    if (r->tileset_textures) {
        for (size_t i = 0; i < r->texture_count; ++i) {
            if (r->tileset_textures[i].id != 0) {
                UnloadTexture(r->tileset_textures[i]);
            }
        }
    }
    free(r->tileset_textures);
    *r = (tiled_renderer_t){0};
}

void tiled_renderer_draw(const tiled_map_t *map, const tiled_renderer_t *r) {
    if (!map || !map->tilesets || map->tileset_count == 0 || !r || !r->tileset_textures || r->texture_count == 0) return;
    int tw = map->tilewidth;
    int th = map->tileheight;

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

                size_t ts_idx = 0;
                const tiled_tileset_t *ts = tileset_for_gid(map, gid, &ts_idx);
                if (!ts || ts_idx >= r->texture_count) continue;

                int local_index = (int)gid - ts->first_gid;
                if (local_index < 0 || local_index >= ts->tilecount) continue;
                int draw_index = animated_tile_index(ts, local_index);
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
                DrawTexturePro(r->tileset_textures[ts_idx], src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
            }
        }
    }
}
