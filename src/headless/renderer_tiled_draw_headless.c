#include "modules/renderer/renderer_internal.h"

bool visible_tile_range(const world_map_t* map,
                        Rectangle padded_view,
                        int* out_startX, int* out_startY,
                        int* out_endX, int* out_endY,
                        int* out_visible_tiles)
{
    (void)map;
    (void)padded_view;
    if (out_startX) *out_startX = 0;
    if (out_startY) *out_startY = 0;
    if (out_endX) *out_endX = 0;
    if (out_endY) *out_endY = 0;
    if (out_visible_tiles) *out_visible_tiles = 0;
    return false;
}

void draw_tmx_stack(const world_map_t* map,
                    const render_view_t* view,
                    int startX, int startY, int endX, int endY,
                    double now_ms,
                    painter_queue_ctx_t* painter_ctx)
{
    (void)map;
    (void)view;
    (void)startX;
    (void)startY;
    (void)endX;
    (void)endY;
    (void)now_ms;
    (void)painter_ctx;
}

void draw_world_fallback_tiles(const render_view_t* view)
{
    (void)view;
}
