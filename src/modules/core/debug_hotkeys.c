#include "modules/core/build_config.h"

#if DEBUG_BUILD

#include "modules/renderer/renderer.h"
#include "modules/asset/asset.h"
#include "modules/core/toast.h"
#include "modules/core/input.h"
#include "modules/core/camera.h"
#include "modules/core/logger.h"
#include "modules/core/cmp_print.h"
#include "modules/core/debug_hotkeys.h"
#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs_game.h"
#include "modules/core/engine.h"
#include "modules/world/world.h"
#include "modules/systems/systems_registration.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>

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

    if (input_pressed(in, BTN_DEBUG_RELOAD_TMX)) {
        bool ok = engine_reload_world();
        int w = 0, h = 0;
        world_size_tiles(&w, &h);
        ui_toast(1.0f, "TMX reload: %s (%dx%d tiles)", ok ? "ok" : "failed", w, h);
    }

    static bool s_inspect_mode = false;
    if (input_pressed(in, BTN_DEBUG_INSPECT)) {
        s_inspect_mode = !s_inspect_mode;
        ui_toast(1.0f, "Inspect mode: %s", s_inspect_mode ? "on" : "off");
    }

    if (s_inspect_mode && input_pressed(in, BTN_MOUSE_L)) {
        camera_view_t logical = camera_get_view();
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        Camera2D cam = {
            .target = (Vector2){ logical.center.x, logical.center.y },
            .offset = (Vector2){ sw / 2.0f, sh / 2.0f },
            .rotation = 0.0f,
            .zoom = logical.zoom > 0.0f ? logical.zoom : 1.0f
        };
        Vector2 screen = { in->mouse.x, in->mouse.y };
        Vector2 world = GetScreenToWorld2D(screen, cam);

        int best = -1;
        float best_d2 = 0.0f;
        for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
            if (!ecs_alive_idx(i)) continue;
            uint32_t mask = ecs_mask[i];
            if ((mask & CMP_POS) == 0) continue;

            float cx = cmp_pos[i].x;
            float cy = cmp_pos[i].y;
            float hx = 8.0f;
            float hy = 8.0f;

            if (mask & CMP_COL) {
                if (cmp_col[i].hx > 0.0f) hx = cmp_col[i].hx;
                if (cmp_col[i].hy > 0.0f) hy = cmp_col[i].hy;
            } else if (mask & CMP_SPR) {
                float w = fabsf(cmp_spr[i].src.w);
                float h = fabsf(cmp_spr[i].src.h);
                if (w > 0.0f) hx = w * 0.5f;
                if (h > 0.0f) hy = h * 0.5f;
            }

            if (world.x < cx - hx || world.x > cx + hx ||
                world.y < cy - hy || world.y > cy + hy) {
                continue;
            }

            float dx = world.x - cx;
            float dy = world.y - cy;
            float d2 = dx*dx + dy*dy;
            if (best < 0 || d2 < best_d2) {
                best = i;
                best_d2 = d2;
            }
        }

        if (best >= 0) {
            ecs_entity_t h = handle_from_index(best);
            uint32_t mask = ecs_mask[best];
            LOGC(LOGCAT_ECS, LOG_LVL_INFO,
                 "[inspect] entity=%u gen=%u mask=0x%08X pos=(%.1f, %.1f) click_world=(%.1f, %.1f)",
                 h.idx, h.gen, mask, cmp_pos[best].x, cmp_pos[best].y, world.x, world.y);

            const char* cmp_indent = "  ";
            if (mask & CMP_VEL) {
                cmp_print_velocity(cmp_indent, &cmp_vel[best]);
            }
            if (mask & CMP_COL) {
                cmp_print_collider(cmp_indent, &cmp_col[best]);
            }
            if (mask & CMP_PHYS_BODY) {
                cmp_print_phys_body(cmp_indent, &cmp_phys_body[best]);
            }
            if (mask & CMP_SPR) {
                cmp_print_sprite(cmp_indent, &cmp_spr[best]);
            }
            if (mask & CMP_ANIM) {
                cmp_print_anim(cmp_indent, &cmp_anim[best]);
            }
            if (mask & CMP_PLAYER) {
                cmp_print_player(cmp_indent);
            }
            if (mask & CMP_ITEM) {
                int kind = 0;
                ecs_game_get_item(h, &kind);
                cmp_print_item(cmp_indent, kind);
            }
            if (mask & CMP_INV) {
                int coins = 0;
                bool has_hat = false;
                ecs_game_get_inventory(h, &coins, &has_hat);
                cmp_print_inventory(cmp_indent, coins, has_hat);
            }
            if (mask & CMP_VENDOR) {
                int sells = 0;
                int price = 0;
                ecs_game_get_vendor(h, &sells, &price);
                cmp_print_vendor(cmp_indent, sells, price);
            }
            if (mask & CMP_FOLLOW) {
                cmp_print_follow(cmp_indent, &cmp_follow[best]);
            }
            if (mask & CMP_TRIGGER) {
                cmp_print_trigger(cmp_indent, &cmp_trigger[best]);
            }
            if (mask & CMP_BILLBOARD) {
                cmp_print_billboard(cmp_indent, &cmp_billboard[best]);
            }
            if (mask & CMP_GRAV_GUN) {
                cmp_print_grav_gun(cmp_indent, &cmp_grav_gun[best]);
            }
            if (mask & CMP_DOOR) {
                cmp_print_door(cmp_indent, &cmp_door[best]);
            }
        } else {
            LOGC(LOGCAT_ECS, LOG_LVL_INFO,
                 "[inspect] click_world=(%.1f, %.1f) hit none", world.x, world.y);
        }
    }

    if (input_pressed(in, BTN_DEBUG_FPS)) {
        bool on = renderer_toggle_fps_overlay();
        ui_toast(1.0f, "FPS overlay: %s", on ? "on" : "off");
    }
}

void debug_post_frame(void)
{
    if (!IsKeyPressed(KEY_PRINT_SCREEN)) return;

    const char* dir = "screenshots";
    if (!DirectoryExists(dir)) {
        if (!MakeDirectory(dir)) {
            ui_toast(2.0f, "Screenshot failed: can't create '%s'", dir);
            return;
        }
    }

    static int s_next = 0;
    char path[512];
    for (int attempt = 0; attempt < 10000; ++attempt) {
        int idx = s_next + attempt;
        snprintf(path, sizeof(path), "%s/screenshot_%05d.png", dir, idx);
        if (FileExists(path)) continue;

        TakeScreenshot(path);
        s_next = idx + 1;
        ui_toast(2.0f, "Saved screenshot: %s", path);
        return;
    }

    ui_toast(2.0f, "Screenshot failed: no free filename");
}

SYSTEMS_ADAPT_INPUT(sys_debug_binds_adapt, sys_debug_binds)

#endif // DEBUG_BUILD
