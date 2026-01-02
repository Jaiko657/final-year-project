#include "modules/renderer/renderer_internal.h"

#include <math.h>

unsigned char u8(float x)
{
    if (x < 0.f) x = 0.f;
    if (x > 1.f) x = 1.f;
    return (unsigned char)(x * 255.0f + 0.5f);
}

Rectangle expand_rect(Rectangle r, float margin)
{
    return (Rectangle){
        r.x - margin,
        r.y - margin,
        r.width + 2.0f * margin,
        r.height + 2.0f * margin
    };
}

Rectangle intersect_rect(Rectangle a, Rectangle b)
{
    float nx = fmaxf(a.x, b.x);
    float ny = fmaxf(a.y, b.y);
    float nw = fminf(a.x + a.width,  b.x + b.width)  - nx;
    float nh = fminf(a.y + a.height, b.y + b.height) - ny;
    if (nw <= 0.0f || nh <= 0.0f) {
        return (Rectangle){0};
    }
    return (Rectangle){ nx, ny, nw, nh };
}

bool rects_intersect(Rectangle a, Rectangle b)
{
    return a.x < b.x + b.width && a.x + a.width > b.x &&
           a.y < b.y + b.height && a.y + a.height > b.y;
}

Rectangle sprite_bounds(const ecs_sprite_view_t* v)
{
    float w = fabsf(v->src.w);
    float h = fabsf(v->src.h);
    return (Rectangle){ v->x - v->ox, v->y - v->oy, w, h };
}

rectf rectf_from_rect(Rectangle r)
{
    return (rectf){ r.x, r.y, r.width, r.height };
}
