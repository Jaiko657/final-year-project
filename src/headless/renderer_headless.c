#include "../includes/renderer.h"
#include "../includes/asset.h"
#include "../includes/logger.h"
#include "../includes/tiled.h"

#include <string.h>

static tiled_map_t g_tiled_map;
static bool g_tiled_ready = false;

bool renderer_init(int width, int height, const char* title, int target_fps)
{
    (void)width; (void)height; (void)title; (void)target_fps;
    return true;
}

bool renderer_load_tiled_map(const char* tmx_path)
{
    if (!tmx_path) return false;

    tiled_map_t new_map;
    if (!tiled_load_map(tmx_path, &new_map)) {
        LOGC(LOGCAT_REND, LOG_LVL_ERROR, "headless renderer: failed to load '%s'", tmx_path);
        return false;
    }

    renderer_unload_tiled_map();
    g_tiled_map = new_map;
    g_tiled_ready = true;
    return true;
}

void renderer_unload_tiled_map(void)
{
    if (!g_tiled_ready) return;
    tiled_free_map(&g_tiled_map);
    g_tiled_ready = false;
    memset(&g_tiled_map, 0, sizeof(g_tiled_map));
}

void renderer_next_frame(void)
{
    // Keep asset lifecycle behavior consistent with the real renderer.
    asset_collect();
}

void renderer_shutdown(void)
{
    renderer_unload_tiled_map();
}

tiled_map_t* renderer_get_tiled_map(void)
{
    return g_tiled_ready ? &g_tiled_map : NULL;
}

#if DEBUG_BUILD
bool renderer_toggle_ecs_colliders(void) { return false; }
bool renderer_toggle_phys_colliders(void) { return false; }
bool renderer_toggle_static_colliders(void) { return false; }
bool renderer_toggle_triggers(void) { return false; }
bool renderer_toggle_fps_overlay(void) { return false; }
#endif

