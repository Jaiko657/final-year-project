#include "../includes/engine.h"
#include "../includes/logger.h"
#include "../includes/logger_raylib_adapter.h"
#include "../includes/input.h"
#include "../includes/asset.h"
#include "../includes/ecs.h"
#include "../includes/ecs_game.h"
#include "../includes/ecs_physics.h"
#include "../includes/toast.h"
#include "../includes/renderer.h"
#include "../includes/camera.h"
#include "../includes/world.h"
#include "../includes/world_physics.h"

#include "raylib.h"

static bool engine_init_subsystems(const char *title)
{
    logger_use_raylib();
    log_set_min_level(LOG_LVL_DEBUG);

    ui_toast_init();

    input_init_defaults();
    asset_init();
    ecs_init();
    ecs_register_game_systems();
    if (!world_load_from_tmx("start.tmx", "walls")) {
        LOGC(LOGCAT_MAIN, LOG_LVL_FATAL, "Failed to load world collision");
        return false;
    }
    world_physics_init();
    int world_w = 0, world_h = 0;
    world_size_px(&world_w, &world_h);
    ecs_set_world_size(world_w, world_h);
    camera_init();

    if (!renderer_init(1280, 720, title, 120)) {
        LOGC(LOGCAT_MAIN, LOG_LVL_FATAL, "renderer_init failed");
        return false;
    }

    if (!renderer_load_tiled_map("start.tmx")) {
        LOGC(LOGCAT_MAIN, LOG_LVL_FATAL, "Failed to load TMX map");
        return false;
    }

    // game entities/assets
    int texture_success = init_entities(32*10, 32*10);
    if (texture_success != 0) {
        LOGC(LOGCAT_MAIN, LOG_LVL_FATAL, "init_entities failed (%d)", texture_success);
        return false;
    }

    camera_config_t cam_cfg = camera_get_config();
    cam_cfg.target   = ecs_find_player();
    v2f spawn = world_get_spawn_px();
    cam_cfg.position = spawn;
    cam_cfg.bounds   = rectf_xywh(0.0f, 0.0f, (float)world_w, (float)world_h);
    cam_cfg.zoom     = 3.0f;
    cam_cfg.stiffness = 25.0f;
    camera_set_config(&cam_cfg);

    return true;
}

bool engine_init(const char *title)
{
    if (!engine_init_subsystems(title)) {
        ecs_shutdown();
        asset_shutdown();
        renderer_shutdown();
        camera_shutdown();
        world_shutdown();
        return false;
    }
    return true;
}

int engine_run(void)
{
    const float FIXED_DT = 1.0f / 60.0f;
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
        ui_toast_update(frame);

        camera_tick(frame);

        ecs_present(frame);
        renderer_next_frame(); // renderer owns asset lifecycle, calls collect, etc.
    }

    return 0;
}

void engine_shutdown(void)
{
    ecs_phys_destroy_all();
    world_physics_shutdown();
    ecs_shutdown();
    asset_shutdown();
    renderer_shutdown();
    camera_shutdown();
    world_shutdown();
}
