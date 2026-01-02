#include "modules/renderer/renderer_internal.h"

#include <math.h>

void enqueue_ecs_sprites(const render_view_t* view, painter_queue_ctx_t* painter_ctx)
{
    if (!view || !painter_ctx || !painter_ctx->queue) return;
    for (ecs_sprite_iter_t it = ecs_sprites_begin(); ; ) {
        ecs_sprite_view_t v;
        if (!ecs_sprites_next(&it, &v)) break;

        Rectangle bounds = sprite_bounds(&v);
        if (!rects_intersect(bounds, view->padded_view)) continue;

        // depth: screen-space "feet"
        float feetY = v.y - v.oy + fabsf(v.src.h);
        Item item = { .v = v, .key = feetY, .seq = (int)painter_ctx->queue->size };
        painter_queue_push(painter_ctx, item);
    }
}
