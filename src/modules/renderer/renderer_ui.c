#include "modules/renderer/renderer_internal.h"
#include "modules/ecs/ecs_game.h"

#include <stdio.h>

void draw_screen_space_ui(const render_view_t* view)
{
    renderer_debug_draw_ui(view);

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
        DrawText("Move: Arrows/WASD | Interact: E | Lift/Throw: C", 10, 36, 18, GRAY);
    }

    // ===== floating billboards (from proximity) =====
    {
        const int fs = 15; // slightly larger text
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        Rectangle screen_bounds = {0, 0, (float)sw, (float)sh};

        for (ecs_billboard_iter_t it = ecs_billboards_begin(); ; ) {
            ecs_billboard_view_t v;
            if (!ecs_billboards_next(&it, &v)) break;

            Vector2 world_pos = { v.x, v.y + v.y_offset };
            Vector2 screen_pos = GetWorldToScreen2D(world_pos, view->cam);

            int tw = MeasureText(v.text, fs);
            float bb_w = (float)(tw + 12);
            float bb_h = 26.0f;
            Rectangle bb = {
                screen_pos.x - (bb_w - 12.0f) / 2.0f,
                screen_pos.y - 6.0f,
                bb_w,
                bb_h
            };

            if (!rects_intersect(bb, screen_bounds)) continue;

            int x = (int)(bb.x + 6.0f);
            int y = (int)screen_pos.y;

            unsigned char a = u8(v.alpha);
            Color bg = (Color){ 0, 0, 0, (unsigned char)(a * 120 / 255) }; // softer background
            Color fg = (Color){ 255, 255, 255, a };

            DrawRectangle((int)bb.x, (int)bb.y, (int)bb.width, (int)bb.height, bg);
            DrawText(v.text, x, y, fs, fg);
        }
    }
}
