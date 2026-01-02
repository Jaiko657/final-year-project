#include "ecs_core_stubs.h"

#include <string.h>

#include "modules/asset/asset.h"
#include "modules/core/camera.h"
#include "modules/core/toast.h"
#include "modules/systems/systems.h"
#include "modules/systems/systems_registration.h"
#include "modules/core/logger.h"
#include "modules/world/world.h"

int g_asset_acquire_calls = 0;
int g_asset_release_calls = 0;
int g_asset_addref_calls = 0;
int g_asset_collect_calls = 0;
bool g_asset_valid_result = true;

int g_phys_create_calls = 0;
int g_phys_destroy_calls = 0;

int g_world_door_register_calls = 0;
int g_world_door_unregister_calls = 0;
int g_world_door_last_count = 0;
door_tile_xy_t g_world_door_last_tiles[4];

int g_world_resolve_mtv_calls = 0;
int g_world_apply_edits_calls = 0;

int g_ecs_register_system_calls = 0;
int g_ecs_phase_calls[PHASE_COUNT] = {0};
int g_ecs_phase_order[16] = {0};
int g_ecs_phase_order_count = 0;

int g_log_warn_calls = 0;
int g_log_error_calls = 0;

void ecs_core_stub_reset(void)
{
    g_asset_acquire_calls = 0;
    g_asset_release_calls = 0;
    g_asset_addref_calls = 0;
    g_asset_collect_calls = 0;
    g_asset_valid_result = true;

    g_phys_create_calls = 0;
    g_phys_destroy_calls = 0;

    g_world_door_register_calls = 0;
    g_world_door_unregister_calls = 0;
    g_world_door_last_count = 0;
    memset(g_world_door_last_tiles, 0, sizeof(g_world_door_last_tiles));

    g_world_resolve_mtv_calls = 0;
    g_world_apply_edits_calls = 0;

    g_ecs_register_system_calls = 0;
    memset(g_ecs_phase_calls, 0, sizeof(g_ecs_phase_calls));
    memset(g_ecs_phase_order, 0, sizeof(g_ecs_phase_order));
    g_ecs_phase_order_count = 0;
    g_log_warn_calls = 0;
    g_log_error_calls = 0;
}

tex_handle_t asset_acquire_texture(const char* path)
{
    (void)path;
    g_asset_acquire_calls++;
    return (tex_handle_t){1, 1};
}

void asset_release_texture(tex_handle_t h)
{
    (void)h;
    g_asset_release_calls++;
}

void asset_addref_texture(tex_handle_t h)
{
    (void)h;
    g_asset_addref_calls++;
}

void asset_collect(void)
{
    g_asset_collect_calls++;
}

bool asset_texture_valid(tex_handle_t h)
{
    (void)h;
    return g_asset_valid_result;
}

void ecs_phys_body_create_for_entity(int idx)
{
    g_phys_create_calls++;
    cmp_phys_body[idx].created = true;
}

void ecs_phys_body_destroy_for_entity(int idx)
{
    g_phys_destroy_calls++;
    cmp_phys_body[idx].created = false;
}

void systems_init(void)
{
}

void systems_register(systems_phase_t phase, int order, systems_fn fn, const char* name)
{
    (void)phase;
    (void)order;
    (void)fn;
    (void)name;
    g_ecs_register_system_calls++;
}

void systems_run_phase(systems_phase_t phase, float dt, const input_t* in)
{
    (void)dt;
    (void)in;
    if ((int)phase >= 0 && phase < PHASE_COUNT) {
        g_ecs_phase_calls[phase]++;
        if (g_ecs_phase_order_count < (int)(sizeof(g_ecs_phase_order) / sizeof(g_ecs_phase_order[0]))) {
            g_ecs_phase_order[g_ecs_phase_order_count++] = phase;
        }
    }
}

void systems_tick(float dt, const input_t* in)
{
    systems_run_phase(PHASE_INPUT, dt, in);
    systems_run_phase(PHASE_SIM_PRE, dt, in);
    systems_run_phase(PHASE_PHYSICS, dt, in);
    systems_run_phase(PHASE_SIM_POST, dt, in);
    systems_run_phase(PHASE_DEBUG, dt, in);
}

void systems_present(float frame_dt)
{
    systems_run_phase(PHASE_PRESENT, frame_dt, NULL);
    systems_run_phase(PHASE_RENDER, frame_dt, NULL);
}

void systems_registration_init(void)
{
    systems_init();
    ecs_register_component_destroy_hook(ENUM_PHYS_BODY, ecs_phys_body_destroy_for_entity);
    ecs_register_phys_body_create_hook(ecs_phys_body_create_for_entity);
    ecs_register_render_component_hooks();
    ecs_register_door_component_hooks();
    systems_register(PHASE_INPUT,    -100, NULL, "effects_tick_begin");
    systems_register(PHASE_INPUT,    0,   NULL, "input");
    systems_register(PHASE_INPUT,    50,  NULL, "grav_gun_input");

    systems_register(PHASE_PHYSICS,  50,  NULL, "follow_ai");
    systems_register(PHASE_PHYSICS,  90,  NULL, "grav_gun_motion");
    systems_register(PHASE_SIM_PRE,  200, NULL, "animation_controller");

    systems_register(PHASE_PHYSICS,  100, NULL, "physics");

    systems_register(PHASE_SIM_POST, 100, NULL, "proximity_view");
    systems_register(PHASE_SIM_POST, 200, NULL, "billboards");
    systems_register(PHASE_SIM_POST, 250, NULL, "grav_gun_fx");
    systems_register(PHASE_SIM_POST, 900, NULL, "world_apply_edits");

    systems_register(PHASE_PRESENT,   10,  NULL, "toast_update");
    systems_register(PHASE_PRESENT,   20,  NULL, "camera_tick");
    systems_register(PHASE_PRESENT,  100, NULL, "sprite_anim");
    systems_register(PHASE_RENDER,     10, NULL, "render_begin");
    systems_register(PHASE_RENDER,     20, NULL, "render_world_base");
    systems_register(PHASE_RENDER,     30, NULL, "render_world_fx");
    systems_register(PHASE_RENDER,     40, NULL, "render_world_sprites");
    systems_register(PHASE_RENDER,     50, NULL, "render_world_overlays");
    systems_register(PHASE_RENDER,     60, NULL, "render_world_end");
    systems_register(PHASE_RENDER,     70, NULL, "render_ui");
    systems_register(PHASE_RENDER,     80, NULL, "render_end");
    systems_register(PHASE_RENDER,   1000, NULL, "asset_collect");

#if DEBUG_BUILD
    systems_register(PHASE_DEBUG,    100, NULL, "debug_binds");
#endif
}

void sys_debug_binds(const input_t* in)
{
    (void)in;
}

void sys_anim_sprite_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_anim_controller_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_prox_build_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_billboards_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_grav_gun_input_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_grav_gun_motion_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void ecs_anim_reset_allocator(void)
{
}

void ecs_anim_shutdown_allocator(void)
{
}

bool world_resolve_rect_mtv_px(float* io_cx, float* io_cy, float hx, float hy)
{
    (void)io_cx;
    (void)io_cy;
    (void)hx;
    (void)hy;
    g_world_resolve_mtv_calls++;
    return false;
}

int world_subtile_size(void)
{
    return 16;
}

bool world_has_line_of_sight(float x0, float y0, float x1, float y1, float max_range, float hx, float hy)
{
    (void)x0; (void)y0; (void)x1; (void)y1; (void)max_range; (void)hx; (void)hy;
    return true;
}

bool world_is_walkable_rect_px(float cx, float cy, float hx, float hy)
{
    (void)cx; (void)cy; (void)hx; (void)hy;
    return true;
}

bool world_resolve_rect_axis_px(float* io_cx, float* io_cy, float hx, float hy, bool axis_x)
{
    (void)io_cx; (void)io_cy; (void)hx; (void)hy; (void)axis_x;
    return false;
}

const world_map_t* world_get_map(void)
{
    return NULL;
}

void world_apply_tile_edits(void)
{
    g_world_apply_edits_calls++;
}

world_door_handle_t world_door_register(const door_tile_xy_t* tile_xy, size_t tile_count)
{
    g_world_door_register_calls++;
    g_world_door_last_count = (int)tile_count;
    if (tile_xy && tile_count <= 4) {
        memcpy(g_world_door_last_tiles, tile_xy, tile_count * sizeof(door_tile_xy_t));
    }
    return 42u;
}

void world_door_unregister(world_door_handle_t handle)
{
    (void)handle;
    g_world_door_unregister_calls++;
}

bool log_would_log(log_level_t lvl)
{
    (void)lvl;
    return true;
}

void log_msg(log_level_t lvl, const log_cat_t* cat, const char* fmt, ...)
{
    (void)cat;
    (void)fmt;
    if (lvl == LOG_LVL_WARN) g_log_warn_calls++;
    if (lvl == LOG_LVL_ERROR) g_log_error_calls++;
}

void camera_tick(float dt)
{
    (void)dt;
}

void ui_toast_update(float dt)
{
    (void)dt;
}
