#include "modules/tiled/tiled.h"
#include "modules/asset/asset.h"
 
#include <stdlib.h>
 
bool tiled_renderer_init(tiled_renderer_t *r, const world_map_t *map)
{
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
 
void tiled_renderer_shutdown(tiled_renderer_t *r)
{
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
