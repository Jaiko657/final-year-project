#include "debug_hotkeys_stubs.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "modules/asset/asset.h"
#include "modules/core/camera.h"
#include "modules/core/engine.h"
#include "modules/ecs/ecs_game.h"
#include "modules/ecs/ecs_internal.h"
#include "modules/core/logger.h"
#include "modules/renderer/renderer.h"
#include "modules/core/toast.h"
#include "modules/world/world_query.h"

int g_asset_reload_calls = 0;
int g_asset_log_calls = 0;
int g_renderer_ecs_calls = 0;
int g_renderer_phys_calls = 0;
int g_renderer_static_calls = 0;
int g_renderer_triggers_calls = 0;
int g_renderer_fps_calls = 0;
int g_toast_calls = 0;
char g_last_toast[256] = {0};
int g_log_calls = 0;
bool g_engine_reload_world_result = false;
int g_world_tiles_w = 0;
int g_world_tiles_h = 0;
bool g_ecs_alive[ECS_MAX_ENTITIES] = {0};
int g_game_storage_plastic = 0;
int g_game_storage_capacity = 0;

void debug_hotkeys_stub_reset(void)
{
    g_asset_reload_calls = 0;
    g_asset_log_calls = 0;
    g_renderer_ecs_calls = 0;
    g_renderer_phys_calls = 0;
    g_renderer_static_calls = 0;
    g_renderer_triggers_calls = 0;
    g_renderer_fps_calls = 0;
    g_toast_calls = 0;
    g_last_toast[0] = '\0';
    g_log_calls = 0;
    g_engine_reload_world_result = false;
    g_world_tiles_w = 0;
    g_world_tiles_h = 0;
    memset(g_ecs_alive, 0, sizeof(g_ecs_alive));
    g_game_storage_plastic = 0;
    g_game_storage_capacity = 0;
}

void asset_reload_all(void)
{
    g_asset_reload_calls++;
}

void asset_log_debug(void)
{
    g_asset_log_calls++;
}

bool renderer_toggle_ecs_colliders(void)
{
    g_renderer_ecs_calls++;
    return (g_renderer_ecs_calls % 2) == 1;
}

bool renderer_toggle_phys_colliders(void)
{
    g_renderer_phys_calls++;
    return (g_renderer_phys_calls % 2) == 1;
}

bool renderer_toggle_static_colliders(void)
{
    g_renderer_static_calls++;
    return (g_renderer_static_calls % 2) == 1;
}

bool renderer_toggle_triggers(void)
{
    g_renderer_triggers_calls++;
    return (g_renderer_triggers_calls % 2) == 1;
}

bool renderer_toggle_fps_overlay(void)
{
    g_renderer_fps_calls++;
    return (g_renderer_fps_calls % 2) == 1;
}

void ui_toast(float secs, const char* fmt, ...)
{
    (void)secs;
    g_toast_calls++;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_last_toast, sizeof(g_last_toast), fmt, ap);
    va_end(ap);
}

bool engine_reload_world(void)
{
    return g_engine_reload_world_result;
}

void world_size_tiles(int* out_w, int* out_h)
{
    if (out_w) *out_w = g_world_tiles_w;
    if (out_h) *out_h = g_world_tiles_h;
}

camera_view_t camera_get_view(void)
{
    camera_view_t view = {0};
    view.zoom = 1.0f;
    return view;
}

bool ecs_game_get_storage(ecs_entity_t e, int* out_plastic, int* out_capacity)
{
    (void)e;
    if (out_plastic) *out_plastic = g_game_storage_plastic;
    if (out_capacity) *out_capacity = g_game_storage_capacity;
    return true;
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
    g_log_calls++;
}

void log_set_sink(log_sink_fn sink)
{
    (void)sink;
}

void log_set_min_level(log_level_t lvl)
{
    (void)lvl;
}

uint32_t ecs_mask[ECS_MAX_ENTITIES];
uint32_t ecs_gen[ECS_MAX_ENTITIES];
uint32_t ecs_next_gen[ECS_MAX_ENTITIES];
cmp_position_t  cmp_pos[ECS_MAX_ENTITIES];
cmp_velocity_t  cmp_vel[ECS_MAX_ENTITIES];
cmp_follow_t    cmp_follow[ECS_MAX_ENTITIES];
cmp_anim_t      cmp_anim[ECS_MAX_ENTITIES];
cmp_sprite_t    cmp_spr[ECS_MAX_ENTITIES];
cmp_collider_t  cmp_col[ECS_MAX_ENTITIES];
cmp_trigger_t   cmp_trigger[ECS_MAX_ENTITIES];
cmp_billboard_t cmp_billboard[ECS_MAX_ENTITIES];
cmp_phys_body_t cmp_phys_body[ECS_MAX_ENTITIES];
cmp_grav_gun_t  cmp_grav_gun[ECS_MAX_ENTITIES];
cmp_door_t      cmp_door[ECS_MAX_ENTITIES];

bool ecs_alive_idx(int i)
{
    if (i < 0 || i >= ECS_MAX_ENTITIES) return false;
    return g_ecs_alive[i];
}

ecs_entity_t handle_from_index(int i)
{
    return (ecs_entity_t){ (uint32_t)i, 0 };
}
