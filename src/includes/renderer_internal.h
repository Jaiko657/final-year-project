#pragma once

#include "engine_types.h"
#include "ecs.h"
#include "dynarray.h"
#include "tiled.h"

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

// ===== forward declarations (ordered by call flow) =====
static render_view_t build_camera_view(void);
static void draw_world(const render_view_t* view);
static void draw_screen_space_ui(const render_view_t* view);

	static bool visible_tile_range(const tiled_map_t* map,
	                               Rectangle padded_view,
	                               int* out_startX, int* out_startY,
	                               int* out_endX, int* out_endY,
	                               int* out_visible_tiles);
static void draw_tmx_stack(const tiled_map_t* map,
                           const render_view_t* view,
                           int startX, int startY, int endX, int endY,
                           painter_queue_ctx_t* painter_ctx);
static void draw_world_fallback_tiles(const render_view_t* view);
static void enqueue_ecs_sprites(const render_view_t* view, painter_queue_ctx_t* painter_ctx);
static void flush_painter_queue(painter_queue_ctx_t* painter_ctx);
static void draw_debug_collision_overlays(const render_view_t* view);
static void draw_debug_trigger_overlays(const render_view_t* view);

static void draw_tile_layer(const tiled_map_t* map,
                            const tiled_renderer_t* tr,
                            const tiled_layer_t* layer,
                            int startX, int startY, int endX, int endY,
                            painter_queue_ctx_t* painter_ctx);
static size_t draw_object_layer_at_z(const tiled_map_t* map,
                                     const tiled_renderer_t* tr,
                                     const render_view_t* view,
                                     painter_queue_ctx_t* painter_ctx,
                                     size_t obj_start,
                                     int target_z);
static bool is_entities_object_layer(const char* layer_name);

static bool painter_queue_push(painter_queue_ctx_t* ctx, Item item);
static int cmp_item(const void* a, const void* b);

#if DEBUG_BUILD && DEBUG_COLLISION
static Rectangle collider_bounds_at(float x, float y, float hx, float hy);
static void draw_collider_outline(Rectangle bounds, const Rectangle* padded_view, Color color);
static void draw_static_colliders(const render_view_t* view, Color color);
static void draw_dynamic_colliders(const render_view_t* view, Color color);
#endif

#if DEBUG_BUILD && DEBUG_TRIGGERS
static Rectangle trigger_bounds(const ecs_trigger_view_t* c);
#endif

static Rectangle camera_view_rect(const Camera2D* cam);
static unsigned char u8(float x);
static Rectangle expand_rect(Rectangle r, float margin);
static Rectangle intersect_rect(Rectangle a, Rectangle b);
static bool rects_intersect(Rectangle a, Rectangle b);
static Rectangle sprite_bounds(const ecs_sprite_view_t* v);
static inline rectf rectf_from_rect(Rectangle r);
static const tiled_tileset_t* tileset_for_gid(const tiled_map_t* map, uint32_t gid, size_t* out_index);
static int animated_tile_index(const tiled_tileset_t *ts, int base_index);
