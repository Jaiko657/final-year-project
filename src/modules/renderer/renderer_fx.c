#include "modules/renderer/renderer_internal.h"
#include "modules/core/effects.h"

#include <math.h>

static Color color_from_colorf(colorf c)
{
    return (Color){ u8(c.r), u8(c.g), u8(c.b), u8(c.a) };
}

void draw_effect_lines(const render_view_t* view)
{
    if (!view) return;

    size_t count = fx_line_count();
    if (count == 0) return;

    for (size_t i = 0; i < count; ++i) {
        const fx_line_t* line = fx_line_at(i);
        if (!line) continue;

        float minx = fminf(line->a.x, line->b.x);
        float miny = fminf(line->a.y, line->b.y);
        float maxx = fmaxf(line->a.x, line->b.x);
        float maxy = fmaxf(line->a.y, line->b.y);
        Rectangle bounds = { minx, miny, maxx - minx, maxy - miny };
        if (!rects_intersect(bounds, view->padded_view)) continue;

        float width = (line->width > 0.0f) ? line->width : 1.0f;
        DrawLineEx(
            (Vector2){ line->a.x, line->a.y },
            (Vector2){ line->b.x, line->b.y },
            width,
            color_from_colorf(line->color)
        );
    }
}
