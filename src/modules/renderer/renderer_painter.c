#include "modules/renderer/renderer_internal.h"
#include "modules/asset/asset_renderer_internal.h"
#include "modules/core/logger.h"

#include <math.h>
#include <stdlib.h>

static Color color_from_colorf(colorf c)
{
    return (Color){ u8(c.r), u8(c.g), u8(c.b), u8(c.a) };
}

static float clampf_local(float v, float lo, float hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

static void draw_sprite_highlight(Texture2D tex, Rectangle src, Rectangle dst, Vector2 origin, colorf color)
{
    if (color.a <= 0.0f) return;

    float t = (sinf((float)GetTime() * 6.0f) + 1.0f) * 0.5f;
    float base_a = clampf_local(color.a, 0.0f, 1.0f);
    float pulse = (60.0f + t * 120.0f) * base_a;
    Color tint = color_from_colorf((colorf){ color.r, color.g, color.b, pulse / 255.0f });

    DrawTexturePro(tex, src, dst, origin, 0.0f, tint);
}

void renderer_painter_prepare(renderer_ctx_t* ctx, int max_items)
{
    if (!ctx) return;
    if (max_items < 0) max_items = 0;
    DA_RESERVE(&ctx->painter_items, (size_t)max_items);
    DA_CLEAR(&ctx->painter_items);
    ctx->painter_ctx = (painter_queue_ctx_t){ .queue = &ctx->painter_items, .dropped = 0 };
    ctx->painter_ready = true;
}

void renderer_painter_ensure_ready(renderer_ctx_t* ctx)
{
    if (!ctx) return;
    if (!ctx->painter_ready) {
        DA_CLEAR(&ctx->painter_items);
        ctx->painter_ctx = (painter_queue_ctx_t){ .queue = &ctx->painter_items, .dropped = 0 };
        ctx->painter_ready = true;
    }
}

bool painter_queue_push(painter_queue_ctx_t* ctx, Item item)
{
    if (!ctx || !ctx->queue) return false;
    if (ctx->queue->size >= ctx->queue->capacity) {
        DA_GROW(ctx->queue);
    }
    if (ctx->queue->size >= ctx->queue->capacity) {
        ctx->dropped++;
        return false;
    }
    ctx->queue->data[ctx->queue->size++] = item;
    return true;
}

static int cmp_item(const void* a, const void* b)
{
    const Item* A = (const Item*)a;
    const Item* B = (const Item*)b;
    if (A->key < B->key) return -1;
    if (A->key > B->key) return  1;
    if (A->seq < B->seq) return -1;
    if (A->seq > B->seq) return  1;
    return 0;
}

void flush_painter_queue(painter_queue_ctx_t* painter_ctx)
{
    if (!painter_ctx || !painter_ctx->queue) return;
    if (painter_ctx->dropped > 0) {
        LOGC(LOGCAT_REND, LOG_LVL_WARN, "painter queue overflow; dropped %d items", painter_ctx->dropped);
    }

    qsort(painter_ctx->queue->data, painter_ctx->queue->size, sizeof(Item), cmp_item);

    for (size_t i = 0; i < painter_ctx->queue->size; ++i) {
        ecs_sprite_view_t v = painter_ctx->queue->data[i].v;

        Texture2D t = asset_backend_resolve_texture_value(v.tex);
        if (t.id == 0) continue;

        Rectangle src = (Rectangle){ v.src.x, v.src.y, v.src.w, v.src.h };
        Rectangle dst = (Rectangle){ v.x, v.y, fabsf(v.src.w), fabsf(v.src.h) };
        Vector2   origin = (Vector2){ v.ox, v.oy };

        DrawTexturePro(t, src, dst, origin, 0.0f, WHITE);
        if (v.highlighted && v.highlight_color.a > 0.0f) {
            draw_sprite_highlight(t, src, dst, origin, v.highlight_color);
        }
    }
}
