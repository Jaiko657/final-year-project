#include "engine_stubs.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "modules/asset/asset.h"
#include "modules/ecs/ecs.h"
#include "modules/ecs/ecs_game.h"
#include "modules/ecs/ecs_physics.h"
#include "modules/core/input.h"
#include "modules/core/logger.h"
#include "modules/core/logger_raylib_adapter.h"
#include "modules/core/platform.h"
#include "modules/renderer/renderer.h"
#include "modules/core/toast.h"
#include "modules/world/world.h"
#include "modules/world/world_query.h"

int g_platform_init_calls = 0;
int g_logger_use_raylib_calls = 0;
int g_log_set_min_level_calls = 0;
int g_ui_toast_init_calls = 0;
int g_ui_toast_update_calls = 0;
int g_input_init_calls = 0;
int g_asset_init_calls = 0;
int g_asset_shutdown_calls = 0;
int g_ecs_init_calls = 0;
int g_ecs_shutdown_calls = 0;
int g_ecs_register_game_systems_calls = 0;
int g_ecs_phys_destroy_all_calls = 0;
int g_renderer_init_calls = 0;
int g_renderer_shutdown_calls = 0;
int g_renderer_bind_calls = 0;
int g_camera_init_calls = 0;
int g_camera_shutdown_calls = 0;
int g_camera_set_config_calls = 0;
int g_world_shutdown_calls = 0;
int g_init_entities_calls = 0;
int g_world_load_calls = 0;
int g_world_load_from_tmx_calls = 0;
int g_input_begin_frame_calls = 0;
int g_input_for_tick_calls = 0;
int g_platform_should_close_calls = 0;
int g_systems_tick_calls = 0;
int g_systems_present_calls = 0;
int g_systems_registration_init_calls = 0;

int g_renderer_init_width = 0;
int g_renderer_init_height = 0;
int g_renderer_init_fps = 0;
char g_renderer_init_title[64] = {0};

char g_world_load_last_path[256] = {0};
bool g_world_load_results[8] = {0};
int g_world_load_result_count = 0;
int g_world_load_result_index = 0;

bool g_renderer_init_result = true;
bool g_renderer_bind_result = true;
bool g_init_entities_result = true;
int g_platform_should_close_after = 0;
float g_time_frame_dt = 0.0f;

int g_world_px_w = 0;
int g_world_px_h = 0;
camera_config_t g_camera_cfg = {0};
camera_view_t g_camera_view = {0};

void engine_stub_reset(void)
{
    g_platform_init_calls = 0;
    g_logger_use_raylib_calls = 0;
    g_log_set_min_level_calls = 0;
    g_ui_toast_init_calls = 0;
    g_ui_toast_update_calls = 0;
    g_input_init_calls = 0;
    g_asset_init_calls = 0;
    g_asset_shutdown_calls = 0;
    g_ecs_init_calls = 0;
    g_ecs_shutdown_calls = 0;
    g_ecs_register_game_systems_calls = 0;
    g_ecs_phys_destroy_all_calls = 0;
    g_renderer_init_calls = 0;
    g_renderer_shutdown_calls = 0;
    g_renderer_bind_calls = 0;
    g_camera_init_calls = 0;
    g_camera_shutdown_calls = 0;
    g_camera_set_config_calls = 0;
    g_world_shutdown_calls = 0;
    g_init_entities_calls = 0;
    g_world_load_calls = 0;
    g_world_load_from_tmx_calls = 0;
    g_input_begin_frame_calls = 0;
    g_input_for_tick_calls = 0;
    g_systems_tick_calls = 0;
    g_systems_present_calls = 0;
    g_systems_registration_init_calls = 0;
    g_platform_should_close_calls = 0;

    g_renderer_init_width = 0;
    g_renderer_init_height = 0;
    g_renderer_init_fps = 0;
    g_renderer_init_title[0] = '\0';

    g_world_load_last_path[0] = '\0';
    memset(g_world_load_results, 0, sizeof(g_world_load_results));
    g_world_load_result_count = 0;
    g_world_load_result_index = 0;

    g_renderer_init_result = true;
    g_renderer_bind_result = true;
    g_init_entities_result = true;
    g_platform_should_close_after = 0;
    g_time_frame_dt = 0.0f;

    g_world_px_w = 0;
    g_world_px_h = 0;
    g_camera_cfg = (camera_config_t){0};
    g_camera_view = (camera_view_t){0};
}

void platform_init(void)
{
    g_platform_init_calls++;
}

bool platform_should_close(void)
{
    g_platform_should_close_calls++;
    return g_platform_should_close_calls > g_platform_should_close_after;
}

void logger_use_raylib(void)
{
    g_logger_use_raylib_calls++;
}

void log_set_min_level(log_level_t lvl)
{
    (void)lvl;
    g_log_set_min_level_calls++;
}

void ui_toast_init(void)
{
    g_ui_toast_init_calls++;
}

void ui_toast_update(float dt)
{
    (void)dt;
    g_ui_toast_update_calls++;
}

void input_init_defaults(void)
{
    g_input_init_calls++;
}

void asset_init(void)
{
    g_asset_init_calls++;
}

void asset_shutdown(void)
{
    g_asset_shutdown_calls++;
}

void ecs_init(void)
{
    g_ecs_init_calls++;
}

void ecs_shutdown(void)
{
    g_ecs_shutdown_calls++;
}

void ecs_register_game_systems(void)
{
    g_ecs_register_game_systems_calls++;
}

void ecs_phys_destroy_all(void)
{
    g_ecs_phys_destroy_all_calls++;
}

bool world_load_from_tmx(const char* tmx_path, const char* collision_layer_name)
{
    (void)collision_layer_name;
    g_world_load_calls++;
    g_world_load_from_tmx_calls++;
    if (tmx_path) {
        snprintf(g_world_load_last_path, sizeof(g_world_load_last_path), "%s", tmx_path);
    } else {
        g_world_load_last_path[0] = '\0';
    }
    if (g_world_load_result_index < g_world_load_result_count) {
        return g_world_load_results[g_world_load_result_index++];
    }
    return true;
}

void world_shutdown(void)
{
    g_world_shutdown_calls++;
}

void world_size_px(int* out_w, int* out_h)
{
    if (out_w) *out_w = g_world_px_w;
    if (out_h) *out_h = g_world_px_h;
}

bool renderer_init(int width, int height, const char* title, int target_fps)
{
    g_renderer_init_calls++;
    g_renderer_init_width = width;
    g_renderer_init_height = height;
    g_renderer_init_fps = target_fps;
    snprintf(g_renderer_init_title, sizeof(g_renderer_init_title), "%s", title ? title : "");
    return g_renderer_init_result;
}

bool renderer_bind_world_map(void)
{
    g_renderer_bind_calls++;
    return g_renderer_bind_result;
}

void renderer_shutdown(void)
{
    g_renderer_shutdown_calls++;
}

void camera_init(void)
{
    g_camera_init_calls++;
}

void camera_shutdown(void)
{
    g_camera_shutdown_calls++;
}

camera_config_t camera_get_config(void)
{
    return g_camera_cfg;
}

void camera_set_config(const camera_config_t* cfg)
{
    g_camera_set_config_calls++;
    if (cfg) g_camera_cfg = *cfg;
}

camera_view_t camera_get_view(void)
{
    return g_camera_view;
}

bool init_entities(const char* tmx_path)
{
    (void)tmx_path;
    g_init_entities_calls++;
    return g_init_entities_result;
}

ecs_entity_t ecs_find_player(void)
{
    return (ecs_entity_t){1u, 1u};
}

bool log_would_log(log_level_t lvl)
{
    (void)lvl;
    return true;
}

void log_msg(log_level_t lvl, const log_cat_t* cat, const char* fmt, ...)
{
    (void)lvl;
    (void)cat;
    (void)fmt;
}

void input_begin_frame(void)
{
    g_input_begin_frame_calls++;
}

input_t input_for_tick(void)
{
    g_input_for_tick_calls++;
    return (input_t){0};
}

void systems_tick(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
    g_systems_tick_calls++;
}

void systems_present(float dt)
{
    (void)dt;
    g_systems_present_calls++;
}

void systems_registration_init(void)
{
    g_systems_registration_init_calls++;
    ecs_register_game_systems();
}

void camera_tick(float dt)
{
    (void)dt;
}

float time_frame_dt(void)
{
    return g_time_frame_dt;
}
