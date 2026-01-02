#include "modules/renderer/renderer_internal.h"
#include "modules/renderer/renderer.h"
#include "modules/core/debug_hotkeys.h"
#include "modules/core/time.h"
#include "modules/systems/systems_registration.h"
#include "modules/world/world.h"
#include "modules/world/world_renderer.h"

static void renderer_world_base_adapt_impl(void)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    renderer_world_base(&ctx->frame_view);
}

static void renderer_world_prepare_adapt_impl(void)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    renderer_world_prepare(&ctx->frame_view);
}

static void renderer_world_fx_adapt_impl(void)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    renderer_world_fx(&ctx->frame_view);
}

static void renderer_world_sprites_adapt_impl(void)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    renderer_world_sprites(&ctx->frame_view);
}

static void renderer_world_overlays_adapt_impl(void)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    renderer_world_overlays(&ctx->frame_view);
}

static void renderer_ui_adapt_impl(void)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    renderer_ui(&ctx->frame_view);
}

SYSTEMS_ADAPT_VOID(sys_render_begin_adapt, renderer_frame_begin)
SYSTEMS_ADAPT_VOID(sys_render_world_prepare_adapt, renderer_world_prepare_adapt_impl)
SYSTEMS_ADAPT_VOID(sys_render_world_base_adapt, renderer_world_base_adapt_impl)
SYSTEMS_ADAPT_VOID(sys_render_world_fx_adapt, renderer_world_fx_adapt_impl)
SYSTEMS_ADAPT_VOID(sys_render_world_sprites_adapt, renderer_world_sprites_adapt_impl)
SYSTEMS_ADAPT_VOID(sys_render_world_overlays_adapt, renderer_world_overlays_adapt_impl)
SYSTEMS_ADAPT_VOID(sys_render_world_end_adapt, renderer_world_end)
SYSTEMS_ADAPT_VOID(sys_render_ui_adapt, renderer_ui_adapt_impl)
SYSTEMS_ADAPT_VOID(sys_render_end_adapt, renderer_frame_end)

void renderer_frame_begin(void)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    BeginDrawing();
    ClearBackground((Color){ 51, 60, 87, 255 });
    ctx->frame_view = build_camera_view();
    ctx->world_cache = (render_world_cache_t){0};
    ctx->frame_active = true;
    ctx->painter_ready = false;
    BeginMode2D(ctx->frame_view.cam);
}

void renderer_world_prepare(const render_view_t* view)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    if (!view || !ctx->frame_active) return;

    if (ctx->bound_gen != 0 && ctx->bound_gen != world_map_generation()) {
        renderer_bind_world_map();
    }

    render_world_cache_t cache = {0};
    cache.map = world_get_map();
    cache.now_ms = time_now() * 1000.0;
    if (cache.map) {
        cache.has_vis = visible_tile_range(
            cache.map,
            view->padded_view,
            &cache.startX,
            &cache.startY,
            &cache.endX,
            &cache.endY,
            &cache.visible_tiles);
        if (!cache.has_vis) {
            cache.startX = 0;
            cache.startY = 0;
            cache.endX = cache.map->width;
            cache.endY = cache.map->height;
        }
    }

    // ===== painter’s algorithm queue ===== Here instead of begin frame because we need to know map size
    int painter_cap = 0;
    if (cache.has_vis && cache.map) {
        int layer_count = (int)cache.map->layer_count;
        if (layer_count < 1) layer_count = 1;
        painter_cap = cache.visible_tiles * layer_count;
    }
    cache.painter_cap = painter_cap;

    int max_items = ECS_MAX_ENTITIES + painter_cap;
    if (max_items < ECS_MAX_ENTITIES) max_items = ECS_MAX_ENTITIES;
    renderer_painter_prepare(ctx, max_items);

    ctx->world_cache = cache;
}

void renderer_world_base(const render_view_t* view)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    if (!view || !ctx->frame_active) return;

    if (!ctx->painter_ready) {
        renderer_painter_prepare(ctx, ECS_MAX_ENTITIES);
    }

    const render_world_cache_t* cache = &ctx->world_cache;
    if (cache->map) {
        draw_tmx_stack(cache->map, view, cache->startX, cache->startY, cache->endX, cache->endY,
                       cache->now_ms, &ctx->painter_ctx);
    } else {
        draw_world_fallback_tiles(view);
    }
}

void renderer_world_fx(const render_view_t* view)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    if (!view || !ctx->frame_active) return;
    draw_effect_lines(view);
}

void renderer_world_sprites(const render_view_t* view)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    if (!view || !ctx->frame_active) return;

    renderer_painter_ensure_ready(ctx);
    enqueue_ecs_sprites(view, &ctx->painter_ctx);
    flush_painter_queue(&ctx->painter_ctx);
    ctx->painter_ready = false;
}

void renderer_world_overlays(const render_view_t* view)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    if (!view || !ctx->frame_active) return;
    draw_debug_collision_overlays(view);
    draw_debug_trigger_overlays(view);
}

void renderer_world_end(void)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    if (!ctx->frame_active) return;
    EndMode2D();
}

void renderer_ui(const render_view_t* view)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    if (!view || !ctx->frame_active) return;
    draw_screen_space_ui(view);
}

void renderer_frame_end(void)
{
    renderer_ctx_t* ctx = renderer_ctx_get();
    if (!ctx->frame_active) return;
    EndDrawing();
    ctx->frame_active = false;
    debug_post_frame();
}
