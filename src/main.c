#include "raylib.h"
#include "components/ecs.h"
#include "components/input.h"
#include <stdio.h>

/*
 * TODO:
 * - create renderer module, hide raylib types etc
 * - figure out pixel scaling sub pixel movements (allow entities to be off the scale, sub pixel position)
*/

#define TRY_TEX(var, path) do{ \
    var = LoadTexture(path); \
    if ((var.id) == 0){ \
        TraceLog(LOG_ERROR, "Failed to load texture: %s", path); \
        CloseWindow(); \
        return 1; \
    } \
}while(0)

int main(void)
{
    const int W=800, H=450;
    InitWindow(W, H, "raylib + ECS: coins, vendor, hat");
    SetTargetFPS(120);

    input_init_defaults();
    ecs_init();
    ecs_set_world_size(W, H);

    Texture2D tex_player, tex_player_hat, tex_coin, tex_npc;
    TRY_TEX(tex_player,     "assets/player.png");
    TRY_TEX(tex_player_hat, "assets/player_hat.png");
    TRY_TEX(tex_coin,       "assets/coin.png");
    TRY_TEX(tex_npc,        "assets/npc.png");

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

        ecs_render_world();
        ecs_debug_draw();
        ecs_draw_vendor_hints();

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
