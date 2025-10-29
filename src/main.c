#include "raylib.h"
#include "components/ecs.h"
#include "components/input.h"
#include "components/logger.h"
#include "components/logger_raylib_adapter.h"
#include <stdio.h>
#include <math.h>

/*
 * TODO:
 * - create renderer module, hide raylib types etc
 * - figure out pixel scaling sub pixel movements (allow entities to be off the scale, sub pixel position)
*/

// ================= Debug =================
#ifndef DEBUG_COLLISION
#define DEBUG_COLLISION 0
#endif
#ifndef DEBUG_FPS
#define DEBUG_FPS 1
#endif

#define TRY_TEX(var, path) do{ \
    var = LoadTexture(path); \
    if ((var.id) == 0){ \
        TraceLog(LOG_ERROR, "Failed to load texture: %s", path); \
        CloseWindow(); \
        return 1; \
    } \
}while(0)

static Texture2D tex_player, tex_player_hat, tex_coin, tex_npc;

void init(int width, int height)
{
    //Setup Raylib
    InitWindow(width, height, "raylib + ECS: coins, vendor, hat");
    SetTargetFPS(120);
    //Setup Custom Logging
    logger_use_raylib();
    log_set_min_level(LOG_LVL_DEBUG);
    //Setup Input
    input_init_defaults();
    //Init ECS and world
    ecs_init();
    ecs_set_world_size(width, height);
}

int init_entities(int W, int H)
{
    //Init textures for entities
    TRY_TEX(tex_player,     "assets/player.png");
    TRY_TEX(tex_player_hat, "assets/player_hat.png");
    TRY_TEX(tex_coin,       "assets/coin.png");
    TRY_TEX(tex_npc,        "assets/npc.png");
    //TODO: UGLY TMP THING, assset manager and renderer decisions need made to avoid this
    ecs_set_hat_texture(tex_player_hat);
    // Player
    ecs_entity_t player = ecs_create();
    cmp_add_position(player, W/2.0f, H/2.0f);
    cmp_add_velocity(player, 0, 0);
    cmp_add_sprite(player, tex_player,
                   (Rectangle){0,0,(float)tex_player.width,(float)tex_player.height},
                   tex_player.width/2.0f, tex_player.height/2.0f);
    tag_add_player(player);
    cmp_add_inventory(player);
    cmp_add_size(player, 12.0f, 12.0f);
    // Coins
    const Vector2 coinPos[3] = {
        { W/2.0f + 120.0f, H/2.0f },
        { W/2.0f - 160.0f, H/2.0f - 60.0f },
        { W/2.0f +  40.0f, H/2.0f + 90.0f }
    };
    for(int i=0;i<3;++i){
        ecs_entity_t c = ecs_create();
        cmp_add_position(c, coinPos[i].x, coinPos[i].y);
        cmp_add_sprite(c, tex_coin,
                       (Rectangle){0,0,(float)tex_coin.width,(float)tex_coin.height},
                       tex_coin.width/2.0f, tex_coin.height/2.0f);
        cmp_add_item(c, ITEM_COIN);
        cmp_add_size(c, tex_coin.width * 0.5f, tex_coin.height * 0.5f);
    }
    // NPC vendor
    ecs_entity_t npc = ecs_create();
    cmp_add_position(npc, 100.0f, 160.0f);
    cmp_add_sprite(npc, tex_npc,
                   (Rectangle){0,0,(float)tex_npc.width,(float)tex_npc.height},
                   tex_npc.width/2.0f, tex_npc.height/2.0f);
    cmp_add_vendor(npc, ITEM_HAT, 3);
    cmp_add_size(npc, 12.0f, 16.0f);
    return 0;
}

void render(void)
{
    for (ecs_sprite_iter_t it = ecs_sprites_begin(); ; ) {
        ecs_sprite_view_t v;
        if (!ecs_sprites_next(&it, &v)) break;

        DrawTexturePro(
            v.tex,
            v.src,
            (Rectangle){ v.x, v.y, v.src.width, v.src.height },
            (Vector2){ v.ox, v.oy },
            0.0f,
            WHITE
        );
    }
  // Vendor Hints
    {
        int vx, vy; const char* msg = NULL;
        if (ecs_vendor_hint_is_active(&vx, &vy, &msg)) {
            const int fs = 18;
            int tw = MeasureText(msg, fs);
            int x = vx - tw/2;
            int y = vy - 32;

            DrawRectangle(x-6, y-6, tw+12, 26, (Color){0,0,0,160});
            DrawText(msg, x, y, fs, RAYWHITE);
        }
    }
    #if DEBUG_COLLISION
    // --- collider debug outlines ---
    for (ecs_collider_iter_t it = ecs_colliders_begin(); ; )
    {
        ecs_collider_view_t c;
        if (!ecs_colliders_next(&it, &c)) break;

        int rx = (int)floorf(c.x - c.hx);
        int ry = (int)floorf(c.y - c.hy);
        int rw = (int)ceilf(2.f * c.hx);
        int rh = (int)ceilf(2.f * c.hy);
        DrawRectangleLines(rx, ry, rw, rh, RED);
    }
    #endif
    #if DEBUG_FPS
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

    // --- toast ---
    if (ecs_toast_is_active()) {
        const char* t = ecs_toast_get_text();
        const int fs = 20;
        int tw = MeasureText(t, fs);
        int x = (GetScreenWidth() - tw)/2;
        int y = 10;

        DrawRectangle(x-8, y-4, tw+16, 28, (Color){0,0,0,180});
        DrawText(t, x, y, fs, RAYWHITE);
    }
}

int main(void)
{
    const int W=800, H=450;
    init(W, H);
    int texture_success = init_entities(W, H);
    if(texture_success != 0) return texture_success;

    // Fixed timestep
    const float FIXED_DT = 1.0f/60.0f;
    float acc = 0.0f;

    while(!WindowShouldClose()){
        input_begin_frame();
        float frame = GetFrameTime(); if(frame>0.25f) frame=0.25f;
        acc += frame;

        while(acc >= FIXED_DT){
            input_t in = input_for_tick();
            ecs_tick(FIXED_DT, &in);   // const-correct (taking const input_t*)
            acc -= FIXED_DT;
        }

        BeginDrawing();
        ClearBackground((Color){24,24,32,255});

        render();

        
        // HUD
        int coins=0; bool hasHat=false;
        ecs_get_player_stats(&coins, &hasHat);
        char hud[64];
        snprintf(hud, sizeof(hud), "Coins: %d  (%s)", coins, hasHat?"Hat ON":"No hat");
        DrawText(hud, 10, 10, 20, RAYWHITE);
        DrawText("Move: Arrows/WASD | Interact: E/Space", 10, 36, 18, GRAY);

        EndDrawing();
    }

    UnloadTexture(tex_player);
    UnloadTexture(tex_player_hat);
    UnloadTexture(tex_coin);
    UnloadTexture(tex_npc);
    ecs_shutdown();
    CloseWindow();
    return 0;
}
