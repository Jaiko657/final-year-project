#include "../includes/engine.h"

#include "../includes/logger.h"
#include "../includes/logger_raylib_adapter.h"
#include "../includes/input.h"
#include "../includes/asset.h"
#include "../includes/ecs.h"
#include "../includes/ecs_game.h"
#include "../includes/toast.h"
#include "../includes/renderer.h"

#include "raylib.h"

static int g_width  = 0;
static int g_height = 0;

static bool engine_init_subsystems(const char *title)
{
    logger_use_raylib();
    log_set_min_level(LOG_LVL_DEBUG);

    ui_toast_init();

    input_init_defaults();
    asset_init();
    ecs_init();
    ecs_register_game_systems();
    ecs_set_world_size(g_width, g_height);

    if (!renderer_init(g_width, g_height, title, 0)) {
        LOGC(LOGCAT_MAIN, LOG_LVL_FATAL, "renderer_init failed");
        return false;
    }

    // here later you can add:
    // camera_init();
    // world_init();

    // game entities/assets
    int texture_success = init_entities(g_width, g_height);
    if (texture_success != 0) {
        LOGC(LOGCAT_MAIN, LOG_LVL_FATAL, "init_entities failed (%d)", texture_success);
        return false;
    }

    return true;
}

bool engine_init(int width, int height, const char *title)
{
    g_width  = width;
    g_height = height;

    if (!engine_init_subsystems(title)) {
        // clean up whatever got initialised
        ecs_shutdown();
        asset_shutdown();
        renderer_shutdown();
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

        ecs_present(frame);
        renderer_next_frame(); // renderer owns asset lifecycle, calls collect, etc.
    }

    return 0;
}

void engine_shutdown(void)
{
    ecs_shutdown();
    asset_shutdown();
    renderer_shutdown();
}
