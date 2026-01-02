#include "modules/ecs/ecs_internal.h"
#include "modules/core/input.h"
#include "modules/systems/systems_registration.h"
#include "modules/core/effects.h"
#include "modules/renderer/renderer.h"
#include <float.h>
#include <math.h>

static bool grav_gun_get_mouse_world(const input_t* in, v2f* out)
{
    if (!in || !out) return false;
    return renderer_screen_to_world(in->mouse.x, in->mouse.y, &out->x, &out->y);
}

static int find_held_index(ecs_entity_t holder)
{
    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & CMP_GRAV_GUN) == 0) continue;
        cmp_grav_gun_t* g = &cmp_grav_gun[i];
        if (g->state != GRAV_GUN_STATE_HELD) continue;
        if (g->holder.idx == holder.idx && g->holder.gen == holder.gen) {
            return i;
        }
    }
    return -1;
}

static bool grav_gun_hit_test(int idx, float mx, float my, float pad)
{
    if ((ecs_mask[idx] & CMP_POS) == 0) return false;
    const float cx = cmp_pos[idx].x;
    const float cy = cmp_pos[idx].y;

    if (ecs_mask[idx] & CMP_COL) {
        float hx = cmp_col[idx].hx + pad;
        float hy = cmp_col[idx].hy + pad;
        return (mx >= cx - hx && mx <= cx + hx && my >= cy - hy && my <= cy + hy);
    }

    const float dx = mx - cx;
    const float dy = my - cy;
    return (dx * dx + dy * dy) <= (pad * pad);
}

static int find_grab_candidate(int player_idx, v2f mouse_world)
{
    const float px = cmp_pos[player_idx].x;
    const float py = cmp_pos[player_idx].y;

    float best_d2 = FLT_MAX;
    int best_idx = -1;

    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (i == player_idx) continue;
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & (CMP_GRAV_GUN | CMP_POS | CMP_PHYS_BODY)) != (CMP_GRAV_GUN | CMP_POS | CMP_PHYS_BODY)) continue;

        cmp_grav_gun_t* g = &cmp_grav_gun[i];
        if (g->state != GRAV_GUN_STATE_FREE) continue;

        if (cmp_phys_body[i].type == PHYS_STATIC) continue;

        const float pickup_dist = (g->pickup_distance > 0.0f) ? g->pickup_distance : 48.0f;
        const float dxp = cmp_pos[i].x - px;
        const float dyp = cmp_pos[i].y - py;
        if ((dxp * dxp + dyp * dyp) > (pickup_dist * pickup_dist)) continue;

        const float pad = (g->pickup_radius > 0.0f) ? g->pickup_radius : 8.0f;
        if (!grav_gun_hit_test(i, mouse_world.x, mouse_world.y, pad)) continue;

        const float dxm = cmp_pos[i].x - mouse_world.x;
        const float dym = cmp_pos[i].y - mouse_world.y;
        const float d2 = dxm * dxm + dym * dym;
        if (d2 < best_d2) {
            best_d2 = d2;
            best_idx = i;
        }
    }

    return best_idx;
}

static void grav_gun_set_player_filter(int idx, bool ignore_player)
{
    if ((ecs_mask[idx] & CMP_PHYS_BODY) == 0) return;

    cmp_grav_gun_t* g = &cmp_grav_gun[idx];
    if (ignore_player) {
        if (!g->saved_mask_valid) {
            g->saved_mask_bits = cmp_phys_body[idx].mask_bits;
            g->saved_mask_valid = true;
        }
        unsigned int mask = cmp_phys_body[idx].mask_bits;
        if (mask == 0u) mask = 0xFFFFFFFFu;
        mask &= ~PHYS_CAT_PLAYER;
        cmp_phys_body[idx].mask_bits = mask;
    } else if (g->saved_mask_valid) {
        cmp_phys_body[idx].mask_bits = g->saved_mask_bits;
        g->saved_mask_valid = false;
    }
}

static void begin_hold(int idx, ecs_entity_t holder, v2f mouse_world)
{
    cmp_grav_gun_t* g = &cmp_grav_gun[idx];
    g->holder = holder;
    g->state = GRAV_GUN_STATE_HELD;
    g->grab_offset_x = cmp_pos[idx].x - mouse_world.x;
    g->grab_offset_y = cmp_pos[idx].y - mouse_world.y;
    g->hold_vel_x = 0.0f;
    g->hold_vel_y = 0.0f;

    if ((ecs_mask[idx] & CMP_VEL) == 0) {
        cmp_add_velocity(handle_from_index(idx), 0.0f, 0.0f, DIR_SOUTH);
    } else {
        cmp_vel[idx].x = 0.0f;
        cmp_vel[idx].y = 0.0f;
    }

    grav_gun_set_player_filter(idx, true);
}

static void release_hold(int idx)
{
    cmp_grav_gun_t* g = &cmp_grav_gun[idx];
    g->holder = ecs_null();
    g->state = GRAV_GUN_STATE_FREE;
    g->hold_vel_x = 0.0f;
    g->hold_vel_y = 0.0f;
    grav_gun_set_player_filter(idx, false);
}

static void sys_grav_gun_input_impl(const input_t* in)
{
    if (!in) return;

    ecs_entity_t player = find_player_handle();
    int player_idx = ent_index_checked(player);
    if (player_idx < 0 || (ecs_mask[player_idx] & CMP_POS) == 0) return;

    int held_idx = find_held_index(player);
    if (held_idx >= 0) {
        if (!input_down(in, BTN_MOUSE_L)) {
            release_hold(held_idx);
        }
        return;
    }

    if (!input_pressed(in, BTN_MOUSE_L)) return;

    v2f mouse_world = v2f_make(0.0f, 0.0f);
    if (!grav_gun_get_mouse_world(in, &mouse_world)) return;

    int pickup_idx = find_grab_candidate(player_idx, mouse_world);
    if (pickup_idx >= 0) {
        begin_hold(pickup_idx, player, mouse_world);
    }
}

static void update_held(int idx, cmp_grav_gun_t* g, float dt, const input_t* in)
{
    int holder_idx = ent_index_checked(g->holder);
    if (holder_idx < 0 || (ecs_mask[holder_idx] & CMP_POS) == 0) {
        release_hold(idx);
        return;
    }

    if (!in || !input_down(in, BTN_MOUSE_L)) {
        release_hold(idx);
        return;
    }

    const float breakoff = g->breakoff_distance;
    if (breakoff > 0.0f) {
        const float dx = cmp_pos[idx].x - cmp_pos[holder_idx].x;
        const float dy = cmp_pos[idx].y - cmp_pos[holder_idx].y;
        const float dist2 = dx * dx + dy * dy;
        if (dist2 > breakoff * breakoff) {
            release_hold(idx);
            return;
        }
    }

    v2f mouse_world = v2f_make(0.0f, 0.0f);
    if (!grav_gun_get_mouse_world(in, &mouse_world)) return;

    float target_x = mouse_world.x + g->grab_offset_x;
    float target_y = mouse_world.y + g->grab_offset_y;

    const float px = cmp_pos[holder_idx].x;
    const float py = cmp_pos[holder_idx].y;
    const float max_hold = g->max_hold_distance;
    if (max_hold > 0.0f) {
        float dx = target_x - px;
        float dy = target_y - py;
        float dist2 = dx * dx + dy * dy;
        float max2 = max_hold * max_hold;
        if (dist2 > max2 && dist2 > 0.0001f) {
            float dist = sqrtf(dist2);
            float scale = max_hold / dist;
            target_x = px + dx * scale;
            target_y = py + dy * scale;
        }
    }

    const float ex = target_x - cmp_pos[idx].x;
    const float ey = target_y - cmp_pos[idx].y;

    float desired_x = ex * g->follow_gain;
    float desired_y = ey * g->follow_gain;

    float max_speed = g->max_speed;
    if (max_speed > 0.0f) {
        float speed2 = desired_x * desired_x + desired_y * desired_y;
        float max2 = max_speed * max_speed;
        if (speed2 > max2 && speed2 > 0.0001f) {
            float speed = sqrtf(speed2);
            float scale = max_speed / speed;
            desired_x *= scale;
            desired_y *= scale;
        }
    }

    float blend = clampf(g->damping * dt, 0.0f, 1.0f);
    g->hold_vel_x = g->hold_vel_x + (desired_x - g->hold_vel_x) * blend;
    g->hold_vel_y = g->hold_vel_y + (desired_y - g->hold_vel_y) * blend;

    if ((ecs_mask[idx] & CMP_VEL) == 0) {
        cmp_add_velocity(handle_from_index(idx), g->hold_vel_x, g->hold_vel_y, DIR_SOUTH);
    } else {
        cmp_vel[idx].x = g->hold_vel_x;
        cmp_vel[idx].y = g->hold_vel_y;
    }
}

static void sys_grav_gun_motion_impl(float dt, const input_t* in)
{
    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & CMP_GRAV_GUN) == 0) continue;

        cmp_grav_gun_t* g = &cmp_grav_gun[i];
        if (g->state == GRAV_GUN_STATE_HELD) {
            update_held(i, g, dt, in);
        }
    }
}

static void sys_grav_gun_fx_impl(void)
{
    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & (CMP_GRAV_GUN | CMP_POS)) != (CMP_GRAV_GUN | CMP_POS)) continue;

        cmp_grav_gun_t* g = &cmp_grav_gun[i];
        if (g->state != GRAV_GUN_STATE_HELD) continue;

        int holder_idx = ent_index_checked(g->holder);
        if (holder_idx < 0 || (ecs_mask[holder_idx] & CMP_POS) == 0) continue;

        colorf color = (colorf){ 0.470588f, 0.784314f, 1.0f, 1.0f };
        int thickness = 1;
        if (ecs_mask[i] & CMP_SPR) {
            cmp_spr[i].fx.highlighted = true;
            color = cmp_spr[i].fx.highlight_color;
            thickness = cmp_spr[i].fx.highlight_thickness;
        }
        if (thickness < 1) thickness = 1;

        v2f start = v2f_make(cmp_pos[holder_idx].x, cmp_pos[holder_idx].y);
        v2f end = v2f_make(cmp_pos[i].x, cmp_pos[i].y);
        fx_line_push(start, end, (float)thickness, color);
    }
}

SYSTEMS_ADAPT_INPUT(sys_grav_gun_input_adapt, sys_grav_gun_input_impl)
SYSTEMS_ADAPT_BOTH(sys_grav_gun_motion_adapt, sys_grav_gun_motion_impl)
SYSTEMS_ADAPT_VOID(sys_grav_gun_fx_adapt, sys_grav_gun_fx_impl)
