#include "../includes/engine_types.h"
#include "../includes/renderer.h"
#include "../includes/ecs.h"
#include "../includes/ecs_game.h"
#include "../includes/asset.h"
#include "../includes/asset_renderer_internal.h"
#include "../includes/logger.h"
#include "../includes/toast.h"
#include "../includes/camera.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "raylib.h"

typedef struct {
    ecs_sprite_view_t v;
    float key;
    int   seq; //insertion order, tie breaker
} Item;

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

#if DEBUG_COLLISION
static Rectangle collider_bounds(const ecs_collider_view_t* c){
    return (Rectangle){ c->x - c->hx, c->y - c->hy, 2.f * c->hx, 2.f * c->hy };
}
#endif

#if DEBUG_TRIGGERS
static Rectangle trigger_bounds(const ecs_trigger_view_t* c){
    return (Rectangle){ c->x - c->hx - c->pad, c->y - c->hy - c->pad, 2.f * c->hx + 2.f * c->pad, 2.f * c->hy + 2.f * c->pad };
}
#endif

static Rectangle billboard_bounds(const ecs_billboard_view_t* v, int fontSize){
    int tw = MeasureText(v->text, fontSize);
    float x = v->x - tw/2.0f - 6.0f;
    float y = v->y + v->y_offset - 6.0f;
    return (Rectangle){ x, y, (float)(tw + 12), 26.0f };
}

static Rectangle camera_view_rect(const Camera2D* cam){
    float viewW = (float)GetScreenWidth()  / cam->zoom;
    float viewH = (float)GetScreenHeight() / cam->zoom;
    float left  = cam->target.x - cam->offset.x / cam->zoom;
    float top   = cam->target.y - cam->offset.y / cam->zoom;
    return (Rectangle){ left, top, viewW, viewH };
}

typedef struct {
    Camera2D cam;
    Rectangle view;
    Rectangle padded_view;
} render_view_t;

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

static void DrawCheckerboardBackground(Rectangle view, int tileSize, Color c1, Color c2) {
    int startX = (int)floorf(view.x / tileSize);
    int startY = (int)floorf(view.y / tileSize);
    int endX   = (int)ceilf((view.x + view.width)  / tileSize);
    int endY   = (int)ceilf((view.y + view.height) / tileSize);

    for (int y = startY; y < endY; y++) {
        for (int x = startX; x < endX; x++) {
            Color c = ((x + y) % 2 == 0) ? c1 : c2;
            DrawRectangle(x * tileSize, y * tileSize, tileSize, tileSize, c);
        }
    }
}

static void draw_world(const render_view_t* view) {
    int worldW = 0, worldH = 0;
    ecs_get_world_size(&worldW, &worldH);
    Rectangle worldRect = {0.0f, 0.0f, (float)worldW, (float)worldH};
    Rectangle tiledRegion = intersect_rect(worldRect, view->padded_view);
    if (tiledRegion.width > 0.0f && tiledRegion.height > 0.0f) {
        DrawCheckerboardBackground(tiledRegion, 32, LIGHTGRAY, DARKGRAY);
    }

    // ===== painter’s algorithm queue =====
    Item items[ECS_MAX_ENTITIES];
    int count = 0;

    for (ecs_sprite_iter_t it = ecs_sprites_begin(); ; ) {
        ecs_sprite_view_t v;
        if (!ecs_sprites_next(&it, &v)) break;

        Rectangle bounds = sprite_bounds(&v);
        if (!rects_intersect(bounds, view->padded_view)) continue;

        // depth: screen-space "feet"
        float feetY = v.y - v.oy + fabsf(v.src.h);
        Item item = { .v = v, .key = feetY, .seq = count };
        items[count++] = item;
    }
    qsort(items, count, sizeof(Item), cmp_item);

    for (int i = 0; i < count; ++i) {
        ecs_sprite_view_t v = items[i].v;

        Texture2D t = asset_backend_resolve_texture_value(v.tex);
        if (t.id == 0) continue;

        Rectangle src = (Rectangle){ v.src.x, v.src.y, v.src.w, v.src.h };
        Rectangle dst = (Rectangle){ v.x, v.y, v.src.w, v.src.h };
        Vector2   origin = (Vector2){ v.ox, v.oy };

        DrawTexturePro(t, src, dst, origin, 0.0f, WHITE);
    }

    // ===== floating billboards (from proximity) =====
    {
        for (ecs_billboard_iter_t it = ecs_billboards_begin(); ; ) {
            ecs_billboard_view_t v;
            if (!ecs_billboards_next(&it, &v)) break;

            const int fs = 18;
            Rectangle bb = billboard_bounds(&v, fs);
            if (!rects_intersect(bb, view->padded_view)) continue;

            int x = (int)(v.x - (bb.width - 12.0f)/2.0f);
            int y = (int)(v.y + v.y_offset);

            unsigned char a = u8(v.alpha);
            Color bg = (Color){ 0, 0, 0, (unsigned char)(a * 180 / 255) };
            Color fg = (Color){ 255, 255, 255, a };

            DrawRectangle((int)bb.x, (int)bb.y, (int)bb.width, (int)bb.height, bg);
            DrawText(v.text, x, y, fs, fg);
        }
    }

#if DEBUG_COLLISION
    // ===== collider debug outlines =====
    for (ecs_collider_iter_t it = ecs_colliders_begin(); ; ) {
        ecs_collider_view_t c;
        if (!ecs_colliders_next(&it, &c)) break;

        Rectangle bounds = collider_bounds(&c);
        if (!rects_intersect(bounds, view->padded_view)) continue;

        int rx = (int)floorf(bounds.x);
        int ry = (int)floorf(bounds.y);
        int rw = (int)ceilf(bounds.width);
        int rh = (int)ceilf(bounds.height);
        DrawRectangleLines(rx, ry, rw, rh, RED);
    }
#endif

#if DEBUG_TRIGGERS
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
#endif
}

static void draw_screen_space_ui(void) {
#if DEBUG_FPS
    // ===== FPS overlay =====
    {
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
        DrawText("Move: Arrows/WASD | Interact: E", 10, 36, 18, GRAY);
    }
}

void renderer_next_frame(void) {
    BeginDrawing();
    ClearBackground(BLACK);

    render_view_t view = build_camera_view();

    BeginMode2D(view.cam);
    draw_world(&view);
    EndMode2D();

    draw_screen_space_ui();

    // Assets GC after drawing
    asset_collect();

    EndDrawing();
}

void renderer_shutdown(void) {
    if (IsWindowReady()) CloseWindow();
}
