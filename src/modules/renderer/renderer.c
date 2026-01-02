#include "modules/renderer/renderer.h"
#include "modules/renderer/renderer_internal.h"
#include "modules/core/logger.h"
#include "modules/core/camera.h"
#include "modules/world/world.h"
#include "modules/world/world_renderer.h"

static renderer_ctx_t g_renderer = {0};

renderer_ctx_t* renderer_ctx_get(void)
{
    return &g_renderer;
}

bool renderer_init(int width, int height, const char* title, int target_fps)
{
    InitWindow(width, height, title ? title : "Game");
    if (!IsWindowReady()) {
        LOGC(LOGCAT_REND, LOG_LVL_FATAL, "Renderer: window failed to init");
        return false;
    }
    SetTargetFPS(target_fps >= 0 ? target_fps : 60);
#if DEBUG_BUILD
    SetTraceLogLevel(LOG_DEBUG);   // make Raylib print DEBUG+
#else
    SetTraceLogLevel(LOG_WARNING);
#endif
    return true;
}

bool renderer_bind_world_map(void)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    const world_map_t* map = world_get_map();
    if (!map) {
        LOGC(LOGCAT_REND, LOG_LVL_ERROR, "tiled: no world map loaded");
        return false;
    }

    tiled_renderer_t new_tiled_renderer;
    if (!tiled_renderer_init(&new_tiled_renderer, map)) {
        LOGC(LOGCAT_REND, LOG_LVL_ERROR, "tiled: renderer init failed (world map)");
        return false;
    }

    int world_w = 0, world_h = 0;
    world_size_tiles(&world_w, &world_h);
    if (world_w > 0 && world_h > 0 && (world_w != map->width || world_h != map->height)) {
        LOGC(LOGCAT_REND, LOG_LVL_WARN, "tiled: TMX size %dx%d differs from collision map %dx%d", map->width, map->height, world_w, world_h);
    }
    int tw = world_tile_size();
    if (tw > 0 && tw != map->tilewidth) {
        LOGC(LOGCAT_REND, LOG_LVL_WARN, "tiled: TMX tilewidth %d differs from engine tile size %d", map->tilewidth, tw);
    }

    renderer_unload_tiled_map();
    ctx->tiled = new_tiled_renderer;
    ctx->bound_gen = world_map_generation();

    LOGC(LOGCAT_REND, LOG_LVL_INFO, "tiled: bound world map (%dx%d @ %dx%d)", map->width, map->height, map->tilewidth, map->tileheight);
    return true;
}

void renderer_unload_tiled_map(void)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    if (ctx->bound_gen == 0) return;
    tiled_renderer_shutdown(&ctx->tiled);
    ctx->bound_gen = 0;
}

void renderer_shutdown(void)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    renderer_unload_tiled_map();
    DA_FREE(&ctx->painter_items);
    if (IsWindowReady()) CloseWindow();
}

bool renderer_screen_to_world(float screen_x, float screen_y, float* out_x, float* out_y)
{
    if (!out_x || !out_y) return false;

    camera_view_t logical = camera_get_view();
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    if (sw <= 0 || sh <= 0) {
        *out_x = logical.center.x;
        *out_y = logical.center.y;
        return false;
    }

    Camera2D cam = {
        .target   = (Vector2){ logical.center.x, logical.center.y },
        .offset   = (Vector2){ sw / 2.0f, sh / 2.0f },
        .rotation = 0.0f,
        .zoom     = logical.zoom > 0.0f ? logical.zoom : 1.0f
    };

    Vector2 screen = { screen_x, screen_y };
    Vector2 world = GetScreenToWorld2D(screen, cam);
    *out_x = world.x;
    *out_y = world.y;
    return true;
}
