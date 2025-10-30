#include "raylib.h"
#include "modules/ecs.h"
#include "modules/input.h"
#include "modules/logger.h"
#include "modules/logger_raylib_adapter.h"
#include "modules/asset.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>  // for qsort

/*
 * TODO:
 * - create renderer module, hide raylib types etc
 * - figure out pixel scaling sub pixel movements (allow entities to be off the scale, sub pixel position)
*/

// ================= Debug =================
#ifndef DEBUG_COLLISION
#define DEBUG_COLLISION 1
#endif
#ifndef DEBUG_FPS
#define DEBUG_FPS 1
#endif

void init(int width, int height)
{
    //Setup Raylib
    InitWindow(width, height, "raylib + ECS: coins, vendor, hat");
    SetTargetFPS(120);
    //Setup Custom Logging
    SetTraceLogLevel(LOG_DEBUG);   // make Raylib print DEBUG+
    logger_use_raylib();
    log_set_min_level(LOG_LVL_DEBUG);
    //Setup Input
    input_init_defaults();
    asset_init();
    //Init ECS and world
    ecs_init();
    ecs_set_world_size(width, height);
}

int init_entities(int W, int H)
{
    tex_handle_t tex_player     = asset_acquire_texture("assets/player.png");
    tex_handle_t tex_coin       = asset_acquire_texture("assets/coin.png");
    tex_handle_t tex_npc        = asset_acquire_texture("assets/npc.png");

    // Query sizes via asset manager
    int pw=0, ph=0, cw=0, ch=0, nw=0, nh=0;
    asset_texture_size(tex_player, &pw, &ph);
    asset_texture_size(tex_coin, &cw, &ch);
    asset_texture_size(tex_npc, &nw, &nh);

    ecs_entity_t player = ecs_create();
    cmp_add_position(player, W/2.0f, H/2.0f);
    cmp_add_velocity(player, 0, 0);
    cmp_add_sprite_handle(player, tex_player,
        (rectf){0,0,(float)pw,(float)ph},
        pw*0.5f, ph*0.5f);
    tag_add_player(player);
    cmp_add_inventory(player);
    cmp_add_size(player, 12.0f, 12.0f);

    const Vector2 coinPos[3] = {
        { W/2.0f + 120.0f, H/2.0f },
        { W/2.0f - 160.0f, H/2.0f - 60.0f },
        { W/2.0f +  40.0f, H/2.0f + 90.0f }
    };
    for(int i=0;i<3;++i){
        ecs_entity_t c = ecs_create();
        cmp_add_position(c, coinPos[i].x, coinPos[i].y);
        cmp_add_sprite_handle(c, tex_coin,
            (rectf){0,0,(float)cw,(float)ch},
            cw*0.5f, ch*0.5f);
        cmp_add_item(c, ITEM_COIN);
        cmp_add_size(c, cw * 0.5f, ch * 0.5f);
    }

    ecs_entity_t npc = ecs_create();
    cmp_add_position(npc, 100.0f, 160.0f);
    cmp_add_sprite_handle(npc, tex_npc,
        (rectf){0,0,(float)nw,(float)nh},
        nw*0.5f, nh*0.5f);
    cmp_add_vendor(npc, ITEM_HAT, 3);
    cmp_add_size(npc, 12.0f, 16.0f);

    // Release our setup refs (ECS added its own refs)
    asset_release_texture(tex_player);
    asset_release_texture(tex_coin);
    asset_release_texture(tex_npc);
    return 0;
}

typedef struct {
    ecs_sprite_view_t v;
    float key;
} Item;

static int cmp_item(const void *a, const void *b)
{
    float ka = ((const Item*)a)->key;
    float kb = ((const Item*)b)->key;
    if (ka < kb) return -1;
    if (ka > kb) return 1;
    return 0;
}

void render(void)
{
    // TODO: Replace painters algorithm with a bake step (ordering, culling etc maybe)
    Item items[ECS_MAX_ENTITIES];
    int count = 0;

    for (ecs_sprite_iter_t it = ecs_sprites_begin(); ; ) {
        ecs_sprite_view_t v;
        if (!ecs_sprites_next(&it, &v)) break;

        float bottomY = (v.y - v.oy) + v.src.h;  // correct fields and origin
        items[count++] = (Item){ .v = v, .key = bottomY };
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
    asset_collect();
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
        DrawText("Move: Arrows/WASD | Interact: E", 10, 36, 18, GRAY);

        EndDrawing();
    }

    ecs_shutdown();
    asset_shutdown();
    CloseWindow();
    return 0;
}
