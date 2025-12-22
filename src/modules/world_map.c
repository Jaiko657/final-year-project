#include "../includes/world.h"
#include "../includes/world_collision.h"
#include "../includes/logger.h"
#include "../includes/dynarray.h"

#include <stdbool.h>
#include <string.h>

typedef struct {
    int layer_idx;
    int tx;
    int ty;
    uint32_t raw_gid;
} world_tile_edit_t;

static tiled_map_t g_tiled_map = {0};
static bool g_tiled_ready = false;
static DA(world_tile_edit_t) g_tile_edits = {0};
static uint32_t g_map_gen = 0;

static void world_unload_map(void)
{
    if (g_tiled_ready) {
        tiled_free_map(&g_tiled_map);
        g_tiled_ready = false;
    }
}

bool world_load_from_tmx(const char* tmx_path, const char* collision_layer_name)
{
    tiled_map_t new_map;
    if (!tiled_load_map(tmx_path, &new_map)) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "world: failed to load TMX '%s'", tmx_path ? tmx_path : "(null)");
        return false;
    }

    if (!world_collision_build_from_map(&new_map, collision_layer_name)) {
        tiled_free_map(&new_map);
        return false;
    }

    world_unload_map();
    g_tiled_map = new_map;
    g_tiled_ready = true;
    g_map_gen++;

    // Drop any pending edits from the previous map.
    DA_CLEAR(&g_tile_edits);

    LOGC(LOGCAT_MAIN, LOG_LVL_INFO, "world: loaded TMX '%s' (%dx%d)", tmx_path, g_tiled_map.width, g_tiled_map.height);
    return true;
}

void world_shutdown(void)
{
    DA_FREE(&g_tile_edits);
    world_unload_map();
    world_collision_shutdown();
}

const tiled_map_t* world_get_tiled_map(void)
{
    return g_tiled_ready ? &g_tiled_map : NULL;
}

uint32_t world_map_generation(void)
{
    return g_map_gen;
}

bool world_set_tile_gid(int layer_idx, int tx, int ty, uint32_t raw_gid)
{
    if (!g_tiled_ready) return false;
    if (layer_idx < 0 || (size_t)layer_idx >= g_tiled_map.layer_count) return false;
    if (tx < 0 || ty < 0 || tx >= g_tiled_map.width || ty >= g_tiled_map.height) return false;

    tiled_layer_t* layer = &g_tiled_map.layers[(size_t)layer_idx];
    if (tx >= layer->width || ty >= layer->height) return false;
    if (!layer->gids) return false;

    world_tile_edit_t e = { layer_idx, tx, ty, raw_gid };
    DA_APPEND(&g_tile_edits, e);
    return true;
}

void world_apply_tile_edits(void)
{
    if (!g_tiled_ready) {
        DA_CLEAR(&g_tile_edits);
        return;
    }

    for (size_t i = 0; i < g_tile_edits.size; ++i) {
        world_tile_edit_t e = g_tile_edits.data[i];
        if (e.layer_idx < 0 || (size_t)e.layer_idx >= g_tiled_map.layer_count) continue;
        if (e.tx < 0 || e.ty < 0 || e.tx >= g_tiled_map.width || e.ty >= g_tiled_map.height) continue;

        tiled_layer_t* layer = &g_tiled_map.layers[(size_t)e.layer_idx];
        if (!layer->gids) continue;
        if (e.tx >= layer->width || e.ty >= layer->height) continue;

        const size_t idx = (size_t)e.ty * (size_t)layer->width + (size_t)e.tx;
        layer->gids[idx] = e.raw_gid;

        if (layer->collision) {
            world_collision_refresh_tile(&g_tiled_map, e.tx, e.ty);
        }
    }

    DA_CLEAR(&g_tile_edits);
}

#if defined(UNIT_TEST) && UNIT_TEST
bool world_test_set_map(tiled_map_t* map, const char* collision_layer_name)
{
    if (!map) return false;
    if (!world_collision_build_from_map(map, collision_layer_name)) return false;

    world_unload_map();
    g_tiled_map = *map;
    memset(map, 0, sizeof(*map));
    g_tiled_ready = true;
    g_map_gen++;
    DA_CLEAR(&g_tile_edits);
    return true;
}
#endif
