#include "../includes/renderer.h"
#include "../includes/asset.h"
#include "../includes/logger.h"
#include "../includes/world_map.h"

bool renderer_init(int width, int height, const char* title, int target_fps)
{
    (void)width; (void)height; (void)title; (void)target_fps;
    return true;
}

bool renderer_bind_world_map(void)
{
    const tiled_map_t* map = world_get_tiled_map();
    if (!map) {
        LOGC(LOGCAT_REND, LOG_LVL_ERROR, "headless renderer: no world map bound");
        return false;
    }
    return true;
}

void renderer_unload_tiled_map(void)
{
}

void renderer_next_frame(void)
{
    // Keep asset lifecycle behavior consistent with the real renderer.
    asset_collect();
}

void renderer_shutdown(void)
{
}

#if DEBUG_BUILD
bool renderer_toggle_ecs_colliders(void) { return false; }
bool renderer_toggle_phys_colliders(void) { return false; }
bool renderer_toggle_static_colliders(void) { return false; }
bool renderer_toggle_triggers(void) { return false; }
bool renderer_toggle_fps_overlay(void) { return false; }
#endif
