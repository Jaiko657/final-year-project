#include "../includes/build_config.h"

#if DEBUG_BUILD

#include "../includes/renderer.h"
#include "../includes/asset.h"
#include "../includes/toast.h"
#include "../includes/input.h"

void sys_debug_binds(const input_t* in)
{
    if (input_pressed(in, BTN_ASSET_DEBUG_PRINT)) {
        asset_reload_all();
        asset_log_debug();
        ui_toast(1.0f, "Assets reloaded");
    }

    if (input_pressed(in, BTN_DEBUG_COLLIDER_ECS)) {
        bool on = renderer_toggle_ecs_colliders();
        ui_toast(1.0f, "ECS colliders: %s", on ? "on" : "off");
    }

    if (input_pressed(in, BTN_DEBUG_COLLIDER_PHYSICS)) {
        bool on = renderer_toggle_phys_colliders();
        ui_toast(1.0f, "Physics colliders: %s", on ? "on" : "off");
    }

    if (input_pressed(in, BTN_DEBUG_COLLIDER_STATIC)) {
        bool on = renderer_toggle_static_colliders();
        ui_toast(1.0f, "Static colliders: %s", on ? "on" : "off");
    }

    if (input_pressed(in, BTN_DEBUG_TRIGGERS)) {
        bool on = renderer_toggle_triggers();
        ui_toast(1.0f, "Triggers: %s", on ? "on" : "off");
    }

    if (input_pressed(in, BTN_DEBUG_FPS)) {
        bool on = renderer_toggle_fps_overlay();
        ui_toast(1.0f, "FPS overlay: %s", on ? "on" : "off");
    }
}

#endif // DEBUG_BUILD
