#include "ecs_registration_stubs.h"

#include <string.h>

systems_registration_call_t g_systems_registration_calls[32];
int g_systems_registration_call_count = 0;
int g_systems_init_seq = 0;
int g_ecs_render_hooks_seq = 0;
int g_ecs_physics_hooks_seq = 0;
int g_ecs_game_hooks_seq = 0;
int g_ecs_door_hooks_seq = 0;

static int g_call_seq = 0;

void ecs_registration_stubs_reset(void)
{
    memset(g_systems_registration_calls, 0, sizeof(g_systems_registration_calls));
    g_systems_registration_call_count = 0;
    g_systems_init_seq = 0;
    g_ecs_render_hooks_seq = 0;
    g_ecs_physics_hooks_seq = 0;
    g_ecs_game_hooks_seq = 0;
    g_ecs_door_hooks_seq = 0;
    g_call_seq = 0;
}

void systems_init(void)
{
    g_systems_init_seq = ++g_call_seq;
}

void systems_register(systems_phase_t phase, int order, systems_fn fn, const char* name)
{
    int seq = ++g_call_seq;
    if (g_systems_registration_call_count < (int)(sizeof(g_systems_registration_calls) /
            sizeof(g_systems_registration_calls[0])))
    {
        int idx = g_systems_registration_call_count++;
        g_systems_registration_calls[idx] = (systems_registration_call_t){
            .phase = phase,
            .order = order,
            .name = name,
            .fn = fn,
            .seq = seq
        };
    }
}

void ecs_register_render_component_hooks(void)
{
    g_ecs_render_hooks_seq = ++g_call_seq;
}

void ecs_register_physics_component_hooks(void)
{
    g_ecs_physics_hooks_seq = ++g_call_seq;
}

void ecs_register_game_systems(void)
{
    g_ecs_game_hooks_seq = ++g_call_seq;
}

void ecs_register_door_component_hooks(void)
{
    g_ecs_door_hooks_seq = ++g_call_seq;
}

void sys_input(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_follow(float dt)
{
    (void)dt;
}

void sys_physics_integrate_impl(float dt)
{
    (void)dt;
}

void sys_input_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_follow_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_physics_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_anim_controller_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_anim_sprite_adapt(float dt, const input_t* in)
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

void sys_render_begin_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_render_world_prepare_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_render_world_base_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_render_world_fx_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_render_world_sprites_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_render_world_overlays_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_render_world_end_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_render_ui_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_render_end_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_effects_tick_begin_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_grav_gun_fx_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_toast_update_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_camera_tick_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_world_apply_edits_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

void sys_asset_collect_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}

#if DEBUG_BUILD
void sys_debug_binds_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
}
#endif
