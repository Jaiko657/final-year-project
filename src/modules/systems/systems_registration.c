#include "modules/systems/systems.h"
#include "modules/systems/systems_registration.h"
#include "modules/asset/asset.h"
#include "modules/core/camera.h"
#include "modules/ecs/ecs_game.h"
#include "modules/ecs/ecs_internal.h"
#include "modules/renderer/renderer.h"
#include "modules/core/toast.h"
#include "modules/world/world.h"

// Forward declarations for internal ECS logic routines.
void sys_input(float dt, const input_t* in);
void sys_follow(float dt);
void sys_physics_integrate_impl(float dt);

// External registrable adapters defined in ECS submodules.
void sys_anim_controller_adapt(float dt, const input_t* in);
void sys_anim_sprite_adapt(float dt, const input_t* in);
void sys_prox_build_adapt(float dt, const input_t* in);
void sys_billboards_adapt(float dt, const input_t* in);
void sys_liftable_input_adapt(float dt, const input_t* in);
void sys_liftable_motion_adapt(float dt, const input_t* in);

void sys_debug_binds(const input_t* in);

static void sys_input_adapt(float dt, const input_t* in)
{
    sys_input(dt, in);
}

static void sys_follow_adapt(float dt, const input_t* in)
{
    (void)in;
    sys_follow(dt);
}

static void sys_physics_adapt(float dt, const input_t* in)
{
    (void)in;
    sys_physics_integrate_impl(dt);
}

static void sys_toast_update_adapt(float dt, const input_t* in)
{
    (void)in;
    ui_toast_update(dt);
}

static void sys_camera_tick_adapt(float dt, const input_t* in)
{
    (void)in;
    camera_tick(dt);
}

static void sys_world_apply_edits_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
    world_apply_tile_edits();
}

static void sys_renderer_present_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
    renderer_next_frame();
}

static void sys_asset_collect_adapt(float dt, const input_t* in)
{
    (void)dt;
    (void)in;
    asset_collect();
}

#if DEBUG_BUILD
static void sys_debug_binds_adapt(float dt, const input_t* in)
{
    (void)dt;
    sys_debug_binds(in);
}
#endif

void systems_registration_init(void)
{
    systems_init();
    ecs_register_render_component_hooks();
    ecs_register_physics_component_hooks();

    // Pipeline mapping:
    // Input -> Intent -> Physics -> Post-Sim -> Present -> Render -> GC
    //
    // PHASE_INPUT:     input sampling and action intents.
    // PHASE_SIM_PRE:   pre-sim state setup (e.g., animation controller).
    // PHASE_PHYSICS:   motion, physics integration.
    // PHASE_SIM_POST:  post-sim derived views and world edits.
    // PHASE_PRESENT:   UI/camera updates, render, asset GC.

    systems_register(PHASE_INPUT,    0,   sys_input_adapt,         "input");
    systems_register(PHASE_INPUT,    50,  sys_liftable_input_adapt, "liftable_input");

    systems_register(PHASE_SIM_PRE,  100, sys_anim_controller_adapt, "animation_controller");
    systems_register(PHASE_PHYSICS,  50,  sys_follow_adapt,        "follow_ai");
    systems_register(PHASE_PHYSICS,  90,  sys_liftable_motion_adapt, "liftable_motion");

    systems_register(PHASE_PHYSICS,  100, sys_physics_adapt,        "physics");

    systems_register(PHASE_SIM_POST, 100, sys_prox_build_adapt,     "proximity_view");
    systems_register(PHASE_SIM_POST, 200, sys_billboards_adapt,      "billboards");
    systems_register(PHASE_SIM_POST, 900, sys_world_apply_edits_adapt, "world_apply_edits");

    systems_register(PHASE_PRESENT,   10,  sys_toast_update_adapt,   "toast_update");
    systems_register(PHASE_PRESENT,   20,  sys_camera_tick_adapt,    "camera_tick");
    systems_register(PHASE_PRESENT,  100, sys_anim_sprite_adapt,    "sprite_anim");
    systems_register(PHASE_PRESENT, 1000, sys_renderer_present_adapt, "renderer_present");
    systems_register(PHASE_PRESENT, 1100, sys_asset_collect_adapt,   "asset_collect");

#if DEBUG_BUILD
    systems_register(PHASE_DEBUG,    100, sys_debug_binds_adapt,    "debug_binds");
#endif

    ecs_register_game_systems();
    ecs_register_door_component_hooks();

}
