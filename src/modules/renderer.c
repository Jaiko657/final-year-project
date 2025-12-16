#include "../includes/engine_types.h"
#include "../includes/renderer.h"
#include "../includes/ecs.h"
#include "../includes/ecs_game.h"
#include "../includes/asset.h"
#include "../includes/asset_renderer_internal.h"
#include "../includes/logger.h"
#include "../includes/toast.h"
#include "../includes/camera.h"
#include "../includes/world.h"
#include "../includes/world_physics.h"
#include "../includes/tiled.h"
#include "../includes/dynarray.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "raylib.h"

typedef struct {
    ecs_sprite_view_t v;
    float key;
    int   seq; //insertion order, tie breaker
} Item;

typedef DA(Item) ItemArray;

typedef struct {
    ItemArray* queue;
    bool* warned_overflow;
} painter_queue_ctx_t;

static tiled_map_t g_tiled_map;
static tiled_renderer_t g_tiled_renderer;
static bool g_tiled_ready = false;
static ItemArray g_painter_items = {0};

void renderer_unload_tiled_map(void)
{
    if (!g_tiled_ready) return;
    tiled_renderer_shutdown(&g_tiled_renderer);
    tiled_free_map(&g_tiled_map);
    g_tiled_ready = false;
}

bool renderer_load_tiled_map(const char* tmx_path)
{
    if (!tmx_path) return false;

    tiled_map_t new_map;
    tiled_renderer_t new_renderer;
    if (!tiled_load_map(tmx_path, &new_map)) {
        LOGC(LOGCAT_REND, LOG_LVL_ERROR, "tiled: failed to load '%s'", tmx_path);
        return false;
    }
    if (!tiled_renderer_init(&new_renderer, &new_map)) {
        LOGC(LOGCAT_REND, LOG_LVL_ERROR, "tiled: renderer init failed for '%s'", tmx_path);
        tiled_free_map(&new_map);
        return false;
    }

    int world_w = 0, world_h = 0;
    world_size_tiles(&world_w, &world_h);
    if (world_w > 0 && world_h > 0 && (world_w != new_map.width || world_h != new_map.height)) {
        LOGC(LOGCAT_REND, LOG_LVL_WARN, "tiled: TMX size %dx%d differs from collision map %dx%d", new_map.width, new_map.height, world_w, world_h);
    }
    int tw = world_tile_size();
    if (tw > 0 && tw != new_map.tilewidth) {
        LOGC(LOGCAT_REND, LOG_LVL_WARN, "tiled: TMX tilewidth %d differs from engine tile size %d", new_map.tilewidth, tw);
    }

    renderer_unload_tiled_map();
    g_tiled_map = new_map;
    g_tiled_renderer = new_renderer;
    g_tiled_ready = true;

    LOGC(LOGCAT_REND, LOG_LVL_INFO, "tiled: loaded '%s' (%dx%d @ %dx%d)", tmx_path, g_tiled_map.width, g_tiled_map.height, g_tiled_map.tilewidth, g_tiled_map.tileheight);
    return true;
}

static int cmp_item(const void* a, const void* b) {
    const Item* A = (const Item*)a;
    const Item* B = (const Item*)b;
    if (A->key < B->key) return -1;
    if (A->key > B->key) return  1;
    // tiebreaker
    if (A->seq < B->seq) return -1;
    if (A->seq > B->seq) return  1;
    return 0;
}

static bool painter_queue_push(painter_queue_ctx_t* ctx, Item item, const char* warn_msg)
{
    if (!ctx || !ctx->queue) return false;
    if (ctx->queue->size >= ctx->queue->capacity) {
        DA_GROW(ctx->queue);
    }
    if (ctx->queue->size >= ctx->queue->capacity) {
        if (ctx->warned_overflow && warn_msg && !*(ctx->warned_overflow)) {
            LOGC(LOGCAT_REND, LOG_LVL_WARN, "%s", warn_msg);
            *(ctx->warned_overflow) = true;
        }
        return false;
    }
    ctx->queue->data[ctx->queue->size++] = item;
    return true;
}

static unsigned char u8(float x){
    if (x < 0.f) x = 0.f;
    if (x > 1.f) x = 1.f;
    return (unsigned char)(x * 255.0f + 0.5f);
}

static Rectangle expand_rect(Rectangle r, float margin){
    return (Rectangle){
        r.x - margin,
        r.y - margin,
        r.width + 2.0f * margin,
        r.height + 2.0f * margin
    };
}

static Rectangle intersect_rect(Rectangle a, Rectangle b){
    float nx = fmaxf(a.x, b.x);
    float ny = fmaxf(a.y, b.y);
    float nw = fminf(a.x + a.width,  b.x + b.width)  - nx;
    float nh = fminf(a.y + a.height, b.y + b.height) - ny;
    if (nw <= 0.0f || nh <= 0.0f) {
        return (Rectangle){0};
    }
    return (Rectangle){ nx, ny, nw, nh };
}

static bool rects_intersect(Rectangle a, Rectangle b){
    return a.x < b.x + b.width && a.x + a.width > b.x &&
           a.y < b.y + b.height && a.y + a.height > b.y;
}

static Rectangle sprite_bounds(const ecs_sprite_view_t* v){
    float w = fabsf(v->src.w);
    float h = fabsf(v->src.h);
    return (Rectangle){ v->x - v->ox, v->y - v->oy, w, h };
}

static inline rectf rectf_from_rect(Rectangle r){
    return (rectf){ r.x, r.y, r.width, r.height };
}

typedef struct {
    Camera2D cam;
    Rectangle view;
    Rectangle padded_view;
} render_view_t;

#if DEBUG_BUILD && DEBUG_COLLISION
static void draw_collider_outline(Rectangle bounds, const Rectangle* padded_view, Color color);

typedef struct {
    const render_view_t* view;
    Color color;
} static_shape_draw_ctx_t;

static void draw_bb_cb(const cpBB* bb, void* ud)
{
    if (!bb || !ud) return;
    static_shape_draw_ctx_t* c = (static_shape_draw_ctx_t*)ud;
    Rectangle r = {
        (float)bb->l,
        (float)bb->b,
        (float)(bb->r - bb->l),
        (float)(bb->t - bb->b)
    };
    draw_collider_outline(r, &c->view->padded_view, c->color);
}
#endif

#if DEBUG_BUILD && DEBUG_COLLISION
static bool g_draw_ecs_colliders    = false;
static bool g_draw_phys_colliders   = false;
static bool g_draw_static_colliders = false;

static Rectangle collider_bounds_at(float x, float y, float hx, float hy){
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
    if (!world_physics_ready()) return;
    static_shape_draw_ctx_t ctx = { view, color };
    world_physics_for_each_static_shape(draw_bb_cb, &ctx);
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

#if DEBUG_BUILD
bool renderer_toggle_ecs_colliders(void)
{
#if DEBUG_COLLISION
    g_draw_ecs_colliders = !g_draw_ecs_colliders;
    return g_draw_ecs_colliders;
#else
    return false;
#endif
}

bool renderer_toggle_phys_colliders(void)
{
#if DEBUG_COLLISION
    g_draw_phys_colliders = !g_draw_phys_colliders;
    return g_draw_phys_colliders;
#else
    return false;
#endif
}

bool renderer_toggle_static_colliders(void)
{
#if DEBUG_COLLISION
    g_draw_static_colliders = !g_draw_static_colliders;
    return g_draw_static_colliders;
#else
    return false;
#endif
}
#endif

static Rectangle camera_view_rect(const Camera2D* cam){
    float viewW = (float)GetScreenWidth()  / cam->zoom;
    float viewH = (float)GetScreenHeight() / cam->zoom;
    float left  = cam->target.x - cam->offset.x / cam->zoom;
    float top   = cam->target.y - cam->offset.y / cam->zoom;
    return (Rectangle){ left, top, viewW, viewH };
}

static render_view_t build_camera_view(void){
    camera_view_t logical = camera_get_view();
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    Camera2D cam = {
        .target   = (Vector2){ logical.center.x, logical.center.y },
        .offset   = (Vector2){ sw / 2.0f, sh / 2.0f },
        .rotation = 0.0f,
        .zoom     = logical.zoom > 0.0f ? logical.zoom : 1.0f
    };

    Rectangle view = camera_view_rect(&cam);
    float margin = logical.padding >= 0.0f ? logical.padding : 0.0f;
    Rectangle padded = expand_rect(view, margin);

    return (render_view_t){ cam, view, padded };
}

static void enqueue_painter_tile(const tiled_painter_tile_t* tile, void* ud)
{
    if (!tile || !ud) return;
    painter_queue_ctx_t* ctx = (painter_queue_ctx_t*)ud;
    if (!ctx || !ctx->queue) return;

    int idx = (int)ctx->queue->size;
    ecs_sprite_view_t v = {
        .tex = tile->tex,
        .src = rectf_from_rect(tile->src),
        .x   = tile->dst.x,
        .y   = tile->dst.y,
        .ox  = 0.0f,
        .oy  = 0.0f
    };
    float key = tile->dst.y + tile->painter_offset;
    Item item = { .v = v, .key = key, .seq = idx };
    painter_queue_push(ctx, item, "painter queue overflow; dropping painter tiles");
}

#if DEBUG_BUILD && DEBUG_TRIGGERS
static bool g_draw_triggers = false;

static Rectangle trigger_bounds(const ecs_trigger_view_t* c){
    return (Rectangle){ c->x - c->hx - c->pad, c->y - c->hy - c->pad, 2.f * c->hx + 2.f * c->pad, 2.f * c->hy + 2.f * c->pad };
}
#endif

#if DEBUG_BUILD && DEBUG_FPS
static bool g_show_fps = false;
#endif

#if DEBUG_BUILD
bool renderer_toggle_triggers(void)
{
#if DEBUG_TRIGGERS
    g_draw_triggers = !g_draw_triggers;
    return g_draw_triggers;
#else
    return false;
#endif
}

bool renderer_toggle_fps_overlay(void)
{
#if DEBUG_FPS
    g_show_fps = !g_show_fps;
    return g_show_fps;
#else
    return false;
#endif
}
#endif

bool renderer_init(int width, int height, const char* title, int target_fps) {
    InitWindow(width, height, title ? title : "Game");
    if (!IsWindowReady()) {
        LOGC(LOGCAT_REND, LOG_LVL_FATAL, "Renderer: window failed to init");
        return false;
    }
    SetTargetFPS(target_fps >= 0 ? target_fps : 60);
    SetTraceLogLevel(LOG_DEBUG);   // make Raylib print DEBUG+ //TODO: idk if setting up raylib in renderer ideal, because comes with this responsibility
    return true;
}

static void draw_world(const render_view_t* view) {
    // ===== painter’s algorithm queue =====
    int painter_cap = 0;
    if (g_tiled_ready) {
        int tw = g_tiled_map.tilewidth;
        int th = g_tiled_map.tileheight;
        if (tw > 0 && th > 0) {
            Rectangle map_rect = {0.0f, 0.0f, (float)(g_tiled_map.width * tw), (float)(g_tiled_map.height * th)};
            Rectangle vis = intersect_rect(map_rect, view->padded_view);
            if (vis.width > 0.0f && vis.height > 0.0f) {
                int startX = (int)floorf(vis.x / (float)tw);
                int startY = (int)floorf(vis.y / (float)th);
                int endX   = (int)ceilf((vis.x + vis.width) / (float)tw);
                int endY   = (int)ceilf((vis.y + vis.height) / (float)th);
                if (startX < 0) startX = 0;
                if (startY < 0) startY = 0;
                if (endX > g_tiled_map.width) endX = g_tiled_map.width;
                if (endY > g_tiled_map.height) endY = g_tiled_map.height;
                int visible_tiles = (endX - startX) * (endY - startY);
                if (visible_tiles < 0) visible_tiles = 0;
                int layer_count = (int)g_tiled_map.layer_count;
                if (layer_count < 1) layer_count = 1;
                painter_cap = visible_tiles * layer_count;
            }
        }
    }

    int max_items = ECS_MAX_ENTITIES + painter_cap;
    if (max_items < ECS_MAX_ENTITIES) max_items = ECS_MAX_ENTITIES;
    DA_RESERVE(&g_painter_items, (size_t)max_items);
    DA_CLEAR(&g_painter_items);
    bool warned_overflow = false;
    painter_queue_ctx_t painter_ctx = { &g_painter_items, &warned_overflow };

    if (g_tiled_ready) {
        world_sync_tiled_colliders(&g_tiled_map);
        tiled_renderer_draw(&g_tiled_map, &g_tiled_renderer, &view->padded_view, enqueue_painter_tile, &painter_ctx);
    } else {
        int tileSize = world_tile_size();
        int tilesW = 0, tilesH = 0;
        world_size_tiles(&tilesW, &tilesH);

        Rectangle worldRect = {0.0f, 0.0f, (float)(tilesW * tileSize), (float)(tilesH * tileSize)};
        Rectangle visible = intersect_rect(worldRect, view->padded_view);
        if (visible.width > 0.0f && visible.height > 0.0f) {
            int startX = (int)floorf(visible.x / tileSize);
            int startY = (int)floorf(visible.y / tileSize);
            int endX   = (int)ceilf((visible.x + visible.width) / tileSize);
            int endY   = (int)ceilf((visible.y + visible.height) / tileSize);

            if (startX < 0) startX = 0;
            if (startY < 0) startY = 0;
            if (endX > tilesW) endX = tilesW;
            if (endY > tilesH) endY = tilesH;

            for (int ty = startY; ty < endY; ++ty) {
                for (int tx = startX; tx < endX; ++tx) {
                    world_tile_t t = world_tile_at(tx, ty);
                    switch (t) {
                        case WORLD_TILE_WALKABLE: {
                            Color c = ((tx + ty) % 2 == 0) ? LIGHTGRAY : DARKGRAY;
                            DrawRectangle(tx * tileSize, ty * tileSize, tileSize, tileSize, c);
                        } break;
                        case WORLD_TILE_SOLID: {
                            Color c = BROWN;
                            DrawRectangle(tx * tileSize, ty * tileSize, tileSize, tileSize, c);
                        } break;
                        default:
                            break;
                    }
                }
            }
        }
    }

    for (ecs_sprite_iter_t it = ecs_sprites_begin(); ; ) {
        ecs_sprite_view_t v;
        if (!ecs_sprites_next(&it, &v)) break;

        Rectangle bounds = sprite_bounds(&v);
        if (!rects_intersect(bounds, view->padded_view)) continue;

        // depth: screen-space "feet"
        float feetY = v.y - v.oy + fabsf(v.src.h);
        Item item = { .v = v, .key = feetY, .seq = (int)g_painter_items.size };
        painter_queue_push(&painter_ctx, item, "painter queue overflow; dropping sprites");
    }
    qsort(g_painter_items.data, g_painter_items.size, sizeof(Item), cmp_item);

    for (size_t i = 0; i < g_painter_items.size; ++i) {
        ecs_sprite_view_t v = g_painter_items.data[i].v;

        Texture2D t = asset_backend_resolve_texture_value(v.tex);
        if (t.id == 0) continue;

        Rectangle src = (Rectangle){ v.src.x, v.src.y, v.src.w, v.src.h };
        Rectangle dst = (Rectangle){ v.x, v.y, v.src.w, v.src.h };
        Vector2   origin = (Vector2){ v.ox, v.oy };

        DrawTexturePro(t, src, dst, origin, 0.0f, WHITE);
    }

#if DEBUG_COLLISION
    if (g_draw_ecs_colliders || g_draw_phys_colliders || g_draw_static_colliders) {
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
    }
#endif

#if DEBUG_TRIGGERS
    if (g_draw_triggers) {
        // ===== trigger debug outlines =====
        for (ecs_trigger_iter_t it = ecs_triggers_begin(); ; ) {
            ecs_trigger_view_t c;
            if (!ecs_triggers_next(&it, &c)) break;

            Rectangle bounds = trigger_bounds(&c);
            if (!rects_intersect(bounds, view->padded_view)) continue;

            int rx = (int)floorf(bounds.x);
            int ry = (int)floorf(bounds.y);
            int rw = (int)ceilf(bounds.width);
            int rh = (int)ceilf(bounds.height);
            DrawRectangleLines(rx, ry, rw, rh, GREEN);
        }
    }
#endif
}

static void draw_screen_space_ui(const render_view_t* view) {
#if DEBUG_FPS
    // ===== FPS overlay =====
    if (g_show_fps) {
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
    }
#endif

    // ===== toast =====
    if (ecs_toast_is_active()) {
        const char* t = ecs_toast_get_text();
        const int fs = 20;
        int tw = MeasureText(t, fs);
        int x = (GetScreenWidth() - tw)/2;
        int y = 10;

        DrawRectangle(x-8, y-4, tw+16, 28, (Color){0,0,0,180});
        DrawText(t, x, y, fs, RAYWHITE);
    }

    // ===== HUD =====
    {
        int coins=0; bool hasHat=false;
        game_get_player_stats(&coins, &hasHat);
        char hud[64];
        snprintf(hud, sizeof(hud), "Coins: %d  (%s)", coins, hasHat?"Hat ON":"No hat");
        DrawText(hud, 10, 10, 20, RAYWHITE);
        DrawText("Move: Arrows/WASD | Interact: E | Lift/Throw: C", 10, 36, 18, GRAY);
    }

    // ===== floating billboards (from proximity) =====
    {
        const int fs = 15; // slightly larger text
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        Rectangle screen_bounds = {0, 0, (float)sw, (float)sh};

        for (ecs_billboard_iter_t it = ecs_billboards_begin(); ; ) {
            ecs_billboard_view_t v;
            if (!ecs_billboards_next(&it, &v)) break;

            Vector2 world_pos = { v.x, v.y + v.y_offset };
            Vector2 screen_pos = GetWorldToScreen2D(world_pos, view->cam);

            int tw = MeasureText(v.text, fs);
            float bb_w = (float)(tw + 12);
            float bb_h = 26.0f;
            Rectangle bb = {
                screen_pos.x - (bb_w - 12.0f) / 2.0f,
                screen_pos.y - 6.0f,
                bb_w,
                bb_h
            };

            if (!rects_intersect(bb, screen_bounds)) continue;

            int x = (int)(bb.x + 6.0f);
            int y = (int)screen_pos.y;

            unsigned char a = u8(v.alpha);
            Color bg = (Color){ 0, 0, 0, (unsigned char)(a * 120 / 255) }; // softer background
            Color fg = (Color){ 255, 255, 255, a };

            DrawRectangle((int)bb.x, (int)bb.y, (int)bb.width, (int)bb.height, bg);
            DrawText(v.text, x, y, fs, fg);
        }
    }
}

void renderer_next_frame(void) {
    BeginDrawing();
    ClearBackground((Color){ 51, 60, 87, 255 } );

    render_view_t view = build_camera_view();

    BeginMode2D(view.cam);
    draw_world(&view);
    EndMode2D();

    draw_screen_space_ui(&view);

    // Assets GC after drawing
    asset_collect();

    EndDrawing();
}

void renderer_shutdown(void) {
    renderer_unload_tiled_map();
    DA_FREE(&g_painter_items);
    if (IsWindowReady()) CloseWindow();
}

tiled_map_t* renderer_get_tiled_map(void)
{
    if (!g_tiled_ready) return NULL;
    return &g_tiled_map;
}
