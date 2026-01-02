#include "modules/renderer/renderer_internal.h"
#include "modules/core/camera.h"

static Rectangle camera_view_rect(const Camera2D* cam)
{
    float viewW = (float)GetScreenWidth()  / cam->zoom;
    float viewH = (float)GetScreenHeight() / cam->zoom;
    float left  = cam->target.x - cam->offset.x / cam->zoom;
    float top   = cam->target.y - cam->offset.y / cam->zoom;
    return (Rectangle){ left, top, viewW, viewH };
}

render_view_t build_camera_view(void)
{
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
