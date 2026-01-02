#include "modules/renderer/renderer_internal.h"

void renderer_painter_prepare(renderer_ctx_t* ctx, int max_items)
{
    if (!ctx) return;
    (void)max_items;
    ctx->painter_ctx = (painter_queue_ctx_t){0};
    ctx->painter_ready = true;
}

void renderer_painter_ensure_ready(renderer_ctx_t* ctx)
{
    if (!ctx) return;
    ctx->painter_ready = true;
}

bool painter_queue_push(painter_queue_ctx_t* ctx, Item item)
{
    (void)ctx;
    (void)item;
    return false;
}

void flush_painter_queue(painter_queue_ctx_t* painter_ctx)
{
    (void)painter_ctx;
}
