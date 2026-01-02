#include "modules/core/build_config.h"
#include "modules/renderer/renderer.h"
#include "modules/renderer/renderer_internal.h"
#include "modules/world/world.h"

#include <math.h>
#include <stdio.h>

#if DEBUG_BUILD

#if DEBUG_COLLISION
static bool g_draw_ecs_colliders = false;
static bool g_draw_phys_colliders = false;
static bool g_draw_static_colliders = false;
#endif

#if DEBUG_TRIGGERS
static bool g_draw_triggers = false;
#endif

#if DEBUG_FPS
static bool g_show_fps = false;
#endif

#if DEBUG_COLLISION
bool renderer_toggle_ecs_colliders(void)
{
    g_draw_ecs_colliders = !g_draw_ecs_colliders;
    return g_draw_ecs_colliders;
}

bool renderer_toggle_phys_colliders(void)
{
    g_draw_phys_colliders = !g_draw_phys_colliders;
    return g_draw_phys_colliders;
}

bool renderer_toggle_static_colliders(void)
{
    g_draw_static_colliders = !g_draw_static_colliders;
    return g_draw_static_colliders;
}
#else
bool renderer_toggle_ecs_colliders(void) { return false; }
bool renderer_toggle_phys_colliders(void) { return false; }
bool renderer_toggle_static_colliders(void) { return false; }
#endif

#if DEBUG_TRIGGERS
bool renderer_toggle_triggers(void)
{
    g_draw_triggers = !g_draw_triggers;
    return g_draw_triggers;
}
#else
bool renderer_toggle_triggers(void) { return false; }
#endif

#if DEBUG_FPS
bool renderer_toggle_fps_overlay(void)
{
    g_show_fps = !g_show_fps;
    return g_show_fps;
}
#else
bool renderer_toggle_fps_overlay(void) { return false; }
#endif

#endif

#if DEBUG_BUILD && DEBUG_COLLISION
static Rectangle collider_bounds_at(float x, float y, float hx, float hy)
{
    return (Rectangle){ x - hx, y - hy, 2.f * hx, 2.f * hy };
}

static void draw_collider_outline(Rectangle bounds, const Rectangle* padded_view, Color color)
{
    if (!rects_intersect(bounds, *padded_view)) return;
    int rx = (int)floorf(bounds.x);
    int ry = (int)floorf(bounds.y);
    int rw = (int)ceilf(bounds.width);
    int rh = (int)ceilf(bounds.height);
    DrawRectangleLines(rx, ry, rw, rh, color);
}

static void draw_static_colliders(const render_view_t* view, Color color)
{
    int tiles_w = 0, tiles_h = 0;
    world_size_tiles(&tiles_w, &tiles_h);
    int tile_px = world_tile_size();
    int subtile_px = world_subtile_size();
    if (tiles_w <= 0 || tiles_h <= 0 || tile_px <= 0 || subtile_px <= 0) return;

    int subtiles_per_tile = tile_px / subtile_px;
    if (subtiles_per_tile <= 0) return;

    for (int ty = 0; ty < tiles_h; ++ty) {
        for (int tx = 0; tx < tiles_w; ++tx) {
            if (world_tile_is_dynamic(tx, ty)) continue;

            Rectangle tile_rect = { (float)(tx * tile_px), (float)(ty * tile_px), (float)tile_px, (float)tile_px };
            if (!rects_intersect(tile_rect, view->padded_view)) continue;

            uint16_t mask = world_subtile_mask_at(tx, ty);
            if (mask == 0) continue;

            for (int sy = 0; sy < subtiles_per_tile; ++sy) {
                for (int sx = 0; sx < subtiles_per_tile; ++sx) {
                    int bit = sy * subtiles_per_tile + sx;
                    if ((mask & (uint16_t)(1u << bit)) == 0) continue;

                    Rectangle r = {
                        tile_rect.x + (float)(sx * subtile_px),
                        tile_rect.y + (float)(sy * subtile_px),
                        (float)subtile_px,
                        (float)subtile_px
                    };
                    draw_collider_outline(r, &view->padded_view, color);
                }
            }
        }
    }
}

static void draw_dynamic_colliders(const render_view_t* view, Color color)
{
    int tiles_w = 0, tiles_h = 0;
    world_size_tiles(&tiles_w, &tiles_h);
    int tile_px = world_tile_size();
    int subtile_px = world_subtile_size();
    if (tiles_w <= 0 || tiles_h <= 0 || tile_px <= 0 || subtile_px <= 0) return;

    int subtiles_per_tile = tile_px / subtile_px;
    if (subtiles_per_tile <= 0) return;

    for (int ty = 0; ty < tiles_h; ++ty) {
        for (int tx = 0; tx < tiles_w; ++tx) {
            if (!world_tile_is_dynamic(tx, ty)) continue;

            Rectangle tile_rect = { (float)(tx * tile_px), (float)(ty * tile_px), (float)tile_px, (float)tile_px };
            if (!rects_intersect(tile_rect, view->padded_view)) continue;

            uint16_t mask = world_subtile_mask_at(tx, ty);
            if (mask == 0) continue;

            for (int sy = 0; sy < subtiles_per_tile; ++sy) {
                for (int sx = 0; sx < subtiles_per_tile; ++sx) {
                    int bit = sy * subtiles_per_tile + sx;
                    if ((mask & (uint16_t)(1u << bit)) == 0) continue;

                    Rectangle r = {
                        tile_rect.x + (float)(sx * subtile_px),
                        tile_rect.y + (float)(sy * subtile_px),
                        (float)subtile_px,
                        (float)subtile_px
                    };
                    draw_collider_outline(r, &view->padded_view, color);
                }
            }
        }
    }
}
#endif

void draw_debug_collision_overlays(const render_view_t* view)
{
#if DEBUG_BUILD && DEBUG_COLLISION
    if (!g_draw_ecs_colliders && !g_draw_phys_colliders && !g_draw_static_colliders) return;
    Color ecs_color = RED;
    Color phys_color = BLUE;
    Color static_color = WHITE;

    if (g_draw_static_colliders) {
        draw_static_colliders(view, static_color);
        draw_dynamic_colliders(view, static_color);
    }

    if (g_draw_ecs_colliders || g_draw_phys_colliders) {
        for (ecs_collider_iter_t it = ecs_colliders_begin(); ; ) {
            ecs_collider_view_t c;
            if (!ecs_colliders_next(&it, &c)) break;

            if (g_draw_ecs_colliders) {
                Rectangle bounds = collider_bounds_at(c.ecs_x, c.ecs_y, c.hx, c.hy);
                draw_collider_outline(bounds, &view->padded_view, ecs_color);
            }

            if (g_draw_phys_colliders && c.has_phys) {
                Rectangle bounds = collider_bounds_at(c.phys_x, c.phys_y, c.hx, c.hy);
                draw_collider_outline(bounds, &view->padded_view, phys_color);
            }
        }
    }
#else
    (void)view;
#endif
}

void draw_debug_trigger_overlays(const render_view_t* view)
{
#if DEBUG_BUILD && DEBUG_TRIGGERS
    if (!g_draw_triggers) return;
    for (ecs_trigger_iter_t it = ecs_triggers_begin(); ; ) {
        ecs_trigger_view_t c;
        if (!ecs_triggers_next(&it, &c)) break;

        Rectangle bounds = (Rectangle){ c.x - c.hx - c.pad, c.y - c.hy - c.pad, 2.f * c.hx + 2.f * c.pad, 2.f * c.hy + 2.f * c.pad };
        if (!rects_intersect(bounds, view->padded_view)) continue;

        int rx = (int)floorf(bounds.x);
        int ry = (int)floorf(bounds.y);
        int rw = (int)ceilf(bounds.width);
        int rh = (int)ceilf(bounds.height);
        DrawRectangleLines(rx, ry, rw, rh, GREEN);
    }
#else
    (void)view;
#endif
}

void renderer_debug_draw_ui(const render_view_t* view)
{
#if DEBUG_BUILD && DEBUG_FPS
    (void)view;
    if (!g_show_fps) return;

    int fps = GetFPS();
    float ms = GetFrameTime() * 1000.0f;
    char buf[64];
    snprintf(buf, sizeof(buf), "FPS: %d | %.2f ms", fps, ms);

    int fs = 18;
    int tw = MeasureText(buf, fs);
    int x = (GetScreenWidth() - tw)/2;
    int y = GetScreenHeight() - fs - 6;

    DrawRectangle(x-8, y-4, tw+16, fs+8, (Color){0,0,0,160});
    DrawText(buf, x, y, fs, RAYWHITE);
#else
    (void)view;
#endif
}
