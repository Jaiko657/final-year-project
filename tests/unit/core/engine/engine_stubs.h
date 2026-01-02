#pragma once

#include <stdbool.h>

#include "modules/core/engine_types.h"
#include "modules/ecs/ecs.h"
#include "modules/core/camera.h"

extern int g_platform_init_calls;
extern int g_logger_use_raylib_calls;
extern int g_log_set_min_level_calls;
extern int g_ui_toast_init_calls;
extern int g_ui_toast_update_calls;
extern int g_input_init_calls;
extern int g_asset_init_calls;
extern int g_asset_shutdown_calls;
extern int g_ecs_init_calls;
extern int g_ecs_shutdown_calls;
extern int g_ecs_register_game_systems_calls;
extern int g_ecs_phys_destroy_all_calls;
extern int g_renderer_init_calls;
extern int g_renderer_shutdown_calls;
extern int g_renderer_bind_calls;
extern int g_camera_init_calls;
extern int g_camera_shutdown_calls;
extern int g_camera_set_config_calls;
extern int g_world_shutdown_calls;
extern int g_init_entities_calls;
extern int g_world_load_calls;
extern int g_world_load_from_tmx_calls;
extern int g_input_begin_frame_calls;
extern int g_input_for_tick_calls;
extern int g_systems_tick_calls;
extern int g_systems_present_calls;
extern int g_systems_registration_init_calls;
extern int g_platform_should_close_calls;

extern int g_renderer_init_width;
extern int g_renderer_init_height;
extern int g_renderer_init_fps;
extern char g_renderer_init_title[64];

extern char g_world_load_last_path[256];
extern bool g_world_load_results[8];
extern int g_world_load_result_count;
extern int g_world_load_result_index;

extern bool g_renderer_init_result;
extern bool g_renderer_bind_result;
extern bool g_init_entities_result;
extern int g_platform_should_close_after;
extern float g_time_frame_dt;

extern int g_world_px_w;
extern int g_world_px_h;
extern camera_config_t g_camera_cfg;
extern camera_view_t g_camera_view;

void engine_stub_reset(void);
