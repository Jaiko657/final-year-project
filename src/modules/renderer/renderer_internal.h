#pragma once

#include "modules/core/engine_types.h"
#include "modules/ecs/ecs.h"
#include "modules/ecs/ecs_render.h"
#include "modules/common/dynarray.h"
#include "modules/tiled/tiled.h"

#include "raylib.h"

typedef struct {
    ecs_sprite_view_t v;
    float key;
    int   seq; // insertion order, tie breaker
} Item;

typedef DA(Item) ItemArray;

typedef struct {
    ItemArray* queue;
    int dropped;
} painter_queue_ctx_t;

typedef struct {
    Camera2D cam;
    Rectangle view;
    Rectangle padded_view;
} render_view_t;

typedef struct {
    const world_map_t* map;
    bool has_vis;
    int startX;
    int startY;
    int endX;
    int endY;
    int visible_tiles;
    int painter_cap;
    double now_ms;
} render_world_cache_t;

typedef struct {
    tiled_renderer_t tiled;
    uint32_t bound_gen;
    ItemArray painter_items;
    render_view_t frame_view;
    render_world_cache_t world_cache;
    painter_queue_ctx_t painter_ctx;
    bool frame_active;
    bool painter_ready;
} renderer_ctx_t;

renderer_ctx_t* renderer_ctx_get(void);

// ===== renderer frame pipeline =====
render_view_t build_camera_view(void);
void renderer_frame_begin(void);
void renderer_frame_end(void);
void renderer_world_prepare(const render_view_t* view);
void renderer_world_base(const render_view_t* view);
void renderer_world_fx(const render_view_t* view);
void renderer_world_sprites(const render_view_t* view);
void renderer_world_overlays(const render_view_t* view);
void renderer_world_end(void);
void renderer_ui(const render_view_t* view);
void draw_screen_space_ui(const render_view_t* view);
void draw_effect_lines(const render_view_t* view);
bool visible_tile_range(const world_map_t* map,
                        Rectangle padded_view,
                        int* out_startX, int* out_startY,
                        int* out_endX, int* out_endY,
                        int* out_visible_tiles);
void draw_tmx_stack(const world_map_t* map,
                    const render_view_t* view,
                    int startX, int startY, int endX, int endY,
                    double now_ms,
                    painter_queue_ctx_t* painter_ctx);
void draw_world_fallback_tiles(const render_view_t* view);
void enqueue_ecs_sprites(const render_view_t* view, painter_queue_ctx_t* painter_ctx);
void flush_painter_queue(painter_queue_ctx_t* painter_ctx);
void renderer_painter_prepare(renderer_ctx_t* ctx, int max_items);
void renderer_painter_ensure_ready(renderer_ctx_t* ctx);
unsigned char u8(float x);
Rectangle expand_rect(Rectangle r, float margin);
Rectangle intersect_rect(Rectangle a, Rectangle b);
bool rects_intersect(Rectangle a, Rectangle b);
Rectangle sprite_bounds(const ecs_sprite_view_t* v);
rectf rectf_from_rect(Rectangle r);
bool painter_queue_push(painter_queue_ctx_t* ctx, Item item);
void draw_debug_collision_overlays(const render_view_t* view);
void draw_debug_trigger_overlays(const render_view_t* view);
void renderer_debug_draw_ui(const render_view_t* view);
