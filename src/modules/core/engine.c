#include "modules/core/engine.h"
#include "modules/core/logger.h"
#include "modules/core/logger_raylib_adapter.h"
#include "modules/core/input.h"
#include "modules/asset/asset.h"
#include "modules/ecs/ecs.h"
#include "modules/ecs/ecs_game.h"
#include "modules/ecs/ecs_physics.h"
#include "modules/core/toast.h"
#include "modules/renderer/renderer.h"
#include "modules/core/camera.h"
#include "modules/world/world.h"
#include "modules/systems/systems.h"
#include "modules/systems/systems_registration.h"
#include "modules/core/platform.h"
#include "modules/core/time.h"

#include <string.h>
#include <math.h>

static char g_current_tmx_path[256] = "assets/maps/start.tmx";

static void remember_tmx_path(const char* path)
{
    if (!path) return;
    strncpy(g_current_tmx_path, path, sizeof(g_current_tmx_path));
    g_current_tmx_path[sizeof(g_current_tmx_path) - 1] = '\0';
}

static void sync_camera_to_world(bool snap_to_center)
{
    int world_w = 0, world_h = 0;
    world_size_px(&world_w, &world_h);
    float center_x = (float)world_w * 0.5f;
    float center_y = (float)world_h * 0.5f;

    camera_config_t cam_cfg = camera_get_config();
    cam_cfg.bounds   = rectf_xywh(0.0f, 0.0f, (float)world_w, (float)world_h);
    cam_cfg.target   = ecs_find_player();
    if (snap_to_center) {
        cam_cfg.position = v2f_make(center_x, center_y);
    } else {
        // Preserve current camera position when hot reloading; clamp to new bounds.
        v2f current = camera_get_view().center;
        cam_cfg.position.x = fmaxf(0.0f, fminf(current.x, (float)world_w));
        cam_cfg.position.y = fmaxf(0.0f, fminf(current.y, (float)world_h));
    }
    camera_set_config(&cam_cfg);
}

static bool reload_world_from_path(const char* tmx_path)
{
    if (!tmx_path) tmx_path = g_current_tmx_path;

    char previous_path[sizeof(g_current_tmx_path)];
    strncpy(previous_path, g_current_tmx_path, sizeof(previous_path));
    previous_path[sizeof(previous_path) - 1] = '\0';

    if (!world_load_from_tmx(tmx_path, "walls")) {
        return false;
    }

    if (!renderer_bind_world_map()) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "Failed to load TMX map '%s' for renderer, reverting", tmx_path);
        if (strcmp(previous_path, tmx_path) != 0) {
            if (!world_load_from_tmx(previous_path, "walls")) {
                LOGC(LOGCAT_MAIN, LOG_LVL_FATAL, "Failed to revert world to previous TMX '%s'", previous_path);
            }
        }
        sync_camera_to_world(true);
        return false;
    }

    sync_camera_to_world(false);
    remember_tmx_path(tmx_path);
    return true;
}

static bool engine_init_subsystems(const char *title)
{
    platform_init();
    logger_use_raylib();
    log_set_min_level(LOG_LVL_DEBUG);

    ui_toast_init();

    input_init_defaults();
    asset_init();
    ecs_init();
    systems_registration_init();
    if (!world_load_from_tmx(g_current_tmx_path, "walls")) {
        LOGC(LOGCAT_MAIN, LOG_LVL_FATAL, "Failed to load world collision");
        return false;
    }
    camera_init();

    if (!renderer_init(1280, 720, title, 0)) {
        LOGC(LOGCAT_MAIN, LOG_LVL_FATAL, "renderer_init failed");
        return false;
    }

    if (!renderer_bind_world_map()) {
        LOGC(LOGCAT_MAIN, LOG_LVL_FATAL, "Failed to load TMX map");
        return false;
    }
    remember_tmx_path(g_current_tmx_path);

    // game entities/assets
    if (!init_entities(g_current_tmx_path)) {
        LOGC(LOGCAT_MAIN, LOG_LVL_FATAL, "init_entities failed");
        return false;
    }

    sync_camera_to_world(true);
    camera_config_t cam_cfg = camera_get_config();
    cam_cfg.zoom       = 3.0f;
    cam_cfg.deadzone_x = 16.0f;
    cam_cfg.deadzone_y = 16.0f;
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

    while (!platform_should_close()) {
        input_begin_frame();

        float frame = time_frame_dt();
        if (frame > 0.25f) frame = 0.25f;  // avoid spiral of death
        acc += frame;

        while (acc >= FIXED_DT) {
            input_t in = input_for_tick();
            systems_tick(FIXED_DT, &in);
            acc -= FIXED_DT;
        }
        systems_present(frame);
    }

    return 0;
}

void engine_shutdown(void)
{
    ecs_phys_destroy_all();
    ecs_shutdown();
    asset_shutdown();
    renderer_shutdown();
    camera_shutdown();
    world_shutdown();
}

bool engine_reload_world(void)
{
    // Keep current camera position when hot-reloading the same TMX; bounds will clamp.
    return reload_world_from_path(g_current_tmx_path);
}

bool engine_reload_world_from_path(const char* tmx_path)
{
    // When reloading a specific TMX (e.g. hot reload), avoid snapping to spawn.
    return reload_world_from_path(tmx_path);
}
