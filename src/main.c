#include "includes/engine_types.h"
#include "includes/renderer.h"
#include "includes/ecs.h"
#include "includes/ecs_game.h"
#include "includes/input.h"
#include "includes/logger.h"
#include "includes/logger_raylib_adapter.h"
#include "includes/asset.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>  // for qsort

#include "raylib.h"

/*
 * TODO:
 * - ENGINE, I dont like that main is managing startup etc of everything, 
 *   I am struggling to think of where state goes other than inside ecs which is not what i want.
 *    I want to have global object that is setup and ran by main, so i can test better also later down the line.
 *      (dep inject, input system, renderer. etc)
 * - figure out pixel scaling sub pixel movements (allow entities to be off the scale, sub pixel position)
*/

int init_entities(int W, int H)
{
    tex_handle_t tex_player     = asset_acquire_texture("assets/player.png");
    tex_handle_t tex_coin       = asset_acquire_texture("assets/coin_gold.png");
    tex_handle_t tex_npc        = asset_acquire_texture("assets/npc.png");

    int pw=16, ph=16, cw=32, ch=32, nw=16, nh=16;

    // ADD PLAYER
    ecs_entity_t player = ecs_create();
    cmp_add_position(player, W/2.0f, H/2.0f);
    cmp_add_velocity(player, 0, 0, DIR_SOUTH);
    cmp_add_sprite_handle(player, tex_player,
        (rectf){0,16,(float)pw,(float)ph},
        pw*0.5f, ph*0.5f);
    cmp_add_player(player);
    cmp_add_inventory(player);
    cmp_add_size(player, 12.0f, 12.0f);
    cmp_add_trigger(player, 2.0f, CMP_ITEM | CMP_COL);
    enum {
        ANIM_WALK_N = 0,
        ANIM_WALK_NE,
        ANIM_WALK_E,
        ANIM_WALK_SE,
        ANIM_WALK_S,
        ANIM_WALK_SW,
        ANIM_WALK_W,
        ANIM_WALK_NW,

        ANIM_IDLE_N,
        ANIM_IDLE_NE,
        ANIM_IDLE_E,
        ANIM_IDLE_SE,
        ANIM_IDLE_S,
        ANIM_IDLE_SW,
        ANIM_IDLE_W,
        ANIM_IDLE_NW,

        ANIM_COUNT
    };
    int frames_per_anim[ANIM_COUNT] = {
        2,2,2,2,2,2,2,2,
        1,1,1,1,1,1,1,1
    };
    anim_frame_coord_t anim_frames[ANIM_COUNT][MAX_FRAMES] = {
        //WALKING
        { {0,0}, {0,2} },
        { {1,0}, {1,2} },
        { {2,0}, {2,2} },
        { {3,0}, {3,2} },
        { {4,0}, {4,2} },
        { {5,0}, {5,2} },
        { {6,0}, {6,2} },
        { {7,0}, {7,2} },
        //IDLE
        { {0,1} },
        { {1,1} },
        { {2,1} },
        { {3,1} },
        { {4,1} },
        { {5,1} },
        { {6,1} },
        { {7,1} },
    };
    cmp_add_anim(
        player,
        16,
        16,
        ANIM_COUNT,
        frames_per_anim,
        anim_frames,
        8.0f
    );

    // ADD COINS
    const v2f coinPos[3] = {
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

        int frames_per_anim[ANIM_COUNT] = {
            8
        };
        anim_frame_coord_t anim_frames[ANIM_COUNT][MAX_FRAMES] = {
            { {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5,0},{6, 0}, {7,0} },
        };
        cmp_add_anim(
            c,
            cw,
            ch,
            ANIM_COUNT,
            frames_per_anim,
            anim_frames,
            8.0f
        );

    }

    // ADD NPC/VENDOR
    ecs_entity_t npc = ecs_create();
    cmp_add_position(npc, 100.0f, H/2.0f);
    cmp_add_sprite_handle(npc, tex_npc,
        (rectf){0,0,(float)nw,(float)nh},
        nw*0.5f, nh*0.5f);
    cmp_add_vendor(npc, ITEM_HAT, 3);
    cmp_add_size(npc, 12.0f, 16.0f);

    cmp_add_trigger(npc, 30.0f, CMP_PLAYER | CMP_COL);
    cmp_add_billboard(npc, "Press E to buy hat", -64.0f, 0.10f, BILLBOARD_ACTIVE);

    // Release setup refs (ECS added its own refs)
    //TODO: I guess this is ok but not sure if i like it
    asset_release_texture(tex_player);
    asset_release_texture(tex_coin);
    asset_release_texture(tex_npc);
    return 0;
}

int main(void)
{
    const int W = 800, H = 450;

    // --- non-window setup ---
    logger_use_raylib();
    log_set_min_level(LOG_LVL_DEBUG);

    input_init_defaults();
    asset_init();
    ecs_init();
    ecs_register_game_systems();
    ecs_set_world_size(W, H);

    // --- renderer/window ---
    if (!renderer_init(W, H, "raylib + ECS: coins, vendor, hat", 0)) {
        LOGC(LOGCAT_MAIN, LOG_LVL_FATAL, "renderer_init failed");
        ecs_shutdown();
        asset_shutdown();
        return 1;
    }

    // --- game entities/assets ---
    int texture_success = init_entities(W, H);
    if (texture_success != 0) {
        renderer_shutdown();
        ecs_shutdown();
        asset_shutdown();
        return texture_success;
    }

    // --- fixed timestep update, render per frame ---
    const float FIXED_DT = 1.0f/60.0f;
    float acc = 0.0f;

    while (!WindowShouldClose()) {
        input_begin_frame();

        float frame = GetFrameTime();
        if (frame > 0.25f) frame = 0.25f;  // avoid spiral of death
        acc += frame;

        while (acc >= FIXED_DT) {
            input_t in = input_for_tick();
            ecs_tick(FIXED_DT, &in);
            acc -= FIXED_DT;
        }
        ecs_present(frame);

        renderer_next_frame(); //renderer owns the asset lifecycle too calling the collect func
    }

    // --- shutdown ---
    ecs_shutdown();
    asset_shutdown();
    renderer_shutdown();
    return 0;
}
