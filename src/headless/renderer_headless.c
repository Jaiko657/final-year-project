#include "modules/renderer/renderer.h"
#include "modules/core/logger.h"
#include "modules/world/world_renderer.h"

bool renderer_init(int width, int height, const char* title, int target_fps)
{
    (void)width; (void)height; (void)title; (void)target_fps;
    return true;
}

bool renderer_bind_world_map(void)
{
    const world_map_t* map = world_get_map();
    if (!map) {
        LOGC(LOGCAT_REND, LOG_LVL_ERROR, "headless renderer: no world map bound");
        return false;
    }
    return true;
}

void renderer_unload_tiled_map(void)
{
}

void renderer_shutdown(void)
{
}

bool renderer_screen_to_world(float screen_x, float screen_y, float* out_x, float* out_y)
{
    (void)screen_x;
    (void)screen_y;
    if (out_x) *out_x = 0.0f;
    if (out_y) *out_y = 0.0f;
    return false;
}
