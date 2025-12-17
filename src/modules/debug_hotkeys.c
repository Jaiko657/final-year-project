#include "../includes/build_config.h"

#if DEBUG_BUILD

#include "../includes/renderer.h"
#include "../includes/asset.h"
#include "../includes/toast.h"
#include "../includes/input.h"
#include "../includes/camera.h"
#include "../includes/logger.h"
#include "../includes/ecs_internal.h"
#include "../includes/engine.h"
#include "../includes/world.h"
#include "raylib.h"
#include <math.h>

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
            if (mask & CMP_VEL) {
                LOGC(LOGCAT_ECS, LOG_LVL_INFO,
                     "  vel=(%.2f, %.2f) facing=%d",
                     cmp_vel[best].x, cmp_vel[best].y, (int)cmp_vel[best].facing.facingDir);
            }
            if (mask & CMP_COL) {
                LOGC(LOGCAT_ECS, LOG_LVL_INFO,
                     "  collider hx=%.1f hy=%.1f", cmp_col[best].hx, cmp_col[best].hy);
            }
            if (mask & CMP_SPR) {
                LOGC(LOGCAT_ECS, LOG_LVL_INFO,
                     "  sprite tex=(%u,%u) src=[%.1f,%.1f,%.1f,%.1f] origin=(%.1f,%.1f)",
                     cmp_spr[best].tex.idx, cmp_spr[best].tex.gen,
                     cmp_spr[best].src.x, cmp_spr[best].src.y, cmp_spr[best].src.w, cmp_spr[best].src.h,
                     cmp_spr[best].ox, cmp_spr[best].oy);
            }
            if (mask & CMP_FOLLOW) {
                LOGC(LOGCAT_ECS, LOG_LVL_INFO,
                     "  follow target=%u dist=%.1f max_speed=%.1f",
                     cmp_follow[best].target.idx, cmp_follow[best].desired_distance, cmp_follow[best].max_speed);
            }
            if (mask & CMP_LIFTABLE) {
                LOGC(LOGCAT_ECS, LOG_LVL_INFO,
                     "  liftable state=%d carrier=%u height=%.1f vz=%.1f",
                     (int)cmp_liftable[best].state, cmp_liftable[best].carrier.idx,
                     cmp_liftable[best].height, cmp_liftable[best].vertical_velocity);
            }
            if (mask & CMP_DOOR) {
                LOGC(LOGCAT_ECS, LOG_LVL_INFO,
                     "  door state=%d tiles=%zu", cmp_door[best].state, cmp_door[best].tiles.size);
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

#endif // DEBUG_BUILD
