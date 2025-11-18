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

static void draw_world(void) {
    // ===== painter’s algorithm queue =====
    Item items[ECS_MAX_ENTITIES];
    int count = 0;

    for (ecs_sprite_iter_t it = ecs_sprites_begin(); ; ) {
        ecs_sprite_view_t v;
        if (!ecs_sprites_next(&it, &v)) break;

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
            int tw = MeasureText(v.text, fs);
            int x = (int)(v.x - tw/2);
            int y = (int)(v.y + v.y_offset);

            unsigned char a = u8(v.alpha);
            Color bg = (Color){ 0, 0, 0, (unsigned char)(a * 180 / 255) };
            Color fg = (Color){ 255, 255, 255, a };

            DrawRectangle(x-6, y-6, tw+12, 26, bg);
            DrawText(v.text, x, y, fs, fg);
        }
    }

#if DEBUG_COLLISION
    // ===== collider debug outlines =====
    for (ecs_collider_iter_t it = ecs_colliders_begin(); ; ) {
        ecs_collider_view_t c;
        if (!ecs_colliders_next(&it, &c)) break;

        int rx = (int)floorf(c.x - c.hx);
        int ry = (int)floorf(c.y - c.hy);
        int rw = (int)ceilf(2.f * c.hx);
        int rh = (int)ceilf(2.f * c.hy);
        DrawRectangleLines(rx, ry, rw, rh, RED);
    }
#endif

#if DEBUG_TRIGGERS
    // ===== trigger debug outlines =====
    for (ecs_trigger_iter_t it = ecs_triggers_begin(); ; ) {
        ecs_trigger_view_t c;
        if (!ecs_triggers_next(&it, &c)) break;

        int rx = (int)floorf(c.x - c.hx - c.pad);
        int ry = (int)floorf(c.y - c.hy - c.pad);
        int rw = (int)ceilf(2.f * c.hx + (2.f * c.pad));
        int rh = (int)ceilf(2.f * c.hy + (2.f * c.pad));
        DrawRectangleLines(rx, ry, rw, rh, GREEN);
    }
#endif

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

}

static void draw_screen_ui(void) {
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

static void DrawWorldBackground(camera_view_t view, int tileSize) {
    const Color outsideColor = BLACK;
    const Color lightTile = (Color){ 205, 205, 205, 255 };
    const Color darkTile = (Color){ 110, 110, 110, 255 };

    const float invZoom = view.zoom != 0.f ? (1.0f / view.zoom) : 1.0f;
    const float halfWidth = (GetScreenWidth() * 0.5f) * invZoom;
    const float halfHeight = (GetScreenHeight() * 0.5f) * invZoom;

    const float left = view.center.x - halfWidth;
    const float top = view.center.y - halfHeight;
    const float right = view.center.x + halfWidth;
    const float bottom = view.center.y + halfHeight;

    DrawRectangleV(
        (Vector2){ left, top },
        (Vector2){ right - left, bottom - top },
        outsideColor);

    if (tileSize <= 0) return;

    int worldW = 0;
    int worldH = 0;
    ecs_get_world_size(&worldW, &worldH);
    if (worldW <= 0 || worldH <= 0) return;

    const float tileLeft = fmaxf(left, 0.0f);
    const float tileTop = fmaxf(top, 0.0f);
    const float tileRight = fminf(right, (float)worldW);
    const float tileBottom = fminf(bottom, (float)worldH);

    if (tileRight <= tileLeft || tileBottom <= tileTop) return;

    const int startX = (int)floorf(tileLeft / tileSize) * tileSize;
    const int startY = (int)floorf(tileTop / tileSize) * tileSize;
    const int endX = (int)ceilf(tileRight / tileSize) * tileSize;
    const int endY = (int)ceilf(tileBottom / tileSize) * tileSize;
    const int startCol = (int)floorf((float)startX / tileSize);
    const int startRow = (int)floorf((float)startY / tileSize);

    for (int y = startY, row = 0; y < endY; y += tileSize, ++row) {
        if (y >= worldH) break;
        int drawY = y;
        if (drawY < 0) continue;
        int tileH = tileSize;
        if (drawY + tileH > worldH) tileH = worldH - drawY;
        if (tileH <= 0) continue;
        const int rowIdx = startRow + row;

        for (int x = startX, col = 0; x < endX; x += tileSize, ++col) {
            if (x >= worldW) break;
            int drawX = x;
            if (drawX < 0) continue;
            int tileW = tileSize;
            if (drawX + tileW > worldW) tileW = worldW - drawX;
            if (tileW <= 0) continue;
            const int colIdx = startCol + col;
            Color c = ((colIdx + rowIdx) & 1) == 0 ? lightTile : darkTile;
            DrawRectangle(drawX, drawY, tileW, tileH, c);
        }
    }
}

void renderer_next_frame(void) {
    BeginDrawing();

    camera_view_t view = camera_get_view();
    Camera2D cam = {
        .target = (Vector2){ view.center.x, view.center.y },
        .offset = (Vector2){ GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f },
        .rotation = 0.0f,
        .zoom = view.zoom,
    };

    BeginMode2D(cam);
    DrawWorldBackground(view, 32);
    draw_world();
    EndMode2D();

    draw_screen_ui();

    // Assets GC after drawing
    asset_collect();

    EndDrawing();
}

void renderer_shutdown(void) {
    if (IsWindowReady()) CloseWindow();
}
