#include "renderer.h"

#include "raylib.h"
#include "ecs.h"
#include "asset.h"
#include "logger.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    ecs_sprite_view_t v;
    float key;
    int   seq; //insertion order
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
    SetTraceLogLevel(LOG_DEBUG);   // make Raylib print DEBUG+
    return true;
}

static void draw_world_and_ui(void) {
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
        ecs_get_player_stats(&coins, &hasHat);
        char hud[64];
        snprintf(hud, sizeof(hud), "Coins: %d  (%s)", coins, hasHat?"Hat ON":"No hat");
        DrawText(hud, 10, 10, 20, RAYWHITE);
        DrawText("Move: Arrows/WASD | Interact: E", 10, 36, 18, GRAY);
    }
}

static void DrawCheckerboardBackground(int tileSize, Color c1, Color c2) {
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();

    // Number of tiles needed to fill the screen
    const int cols = (w + tileSize - 1) / tileSize;
    const int rows = (h + tileSize - 1) / tileSize;

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            Color c = ((x + y) % 2 == 0) ? c1 : c2;
            DrawRectangle(x*tileSize, y*tileSize, tileSize, tileSize, c);
        }
    }
}

void renderer_next_frame(void) {
    BeginDrawing();
    DrawCheckerboardBackground(32, DARKGRAY, BLACK);

    draw_world_and_ui();

    // Assets GC after drawing
    asset_collect();

    EndDrawing();
}

void renderer_shutdown(void) {
    if (IsWindowReady()) CloseWindow();
}
