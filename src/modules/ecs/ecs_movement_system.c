#include "modules/ecs/ecs_internal.h"
#include "modules/world/world.h"
#include <math.h>
#include <stddef.h>

void sys_follow(float dt)
{
    (void)dt;
    const int subtile = world_subtile_size();
    const float stop_at_last_seen = (subtile > 0) ? (float)subtile * 0.35f : 4.0f;
    const float deg_to_rad = 0.01745329251994329577f;
    const float angles[] = {
        0.0f,
        30.0f * deg_to_rad,  -30.0f * deg_to_rad,
        60.0f * deg_to_rad,  -60.0f * deg_to_rad,
        90.0f * deg_to_rad,  -90.0f * deg_to_rad,
        150.0f * deg_to_rad, -150.0f * deg_to_rad,
        180.0f * deg_to_rad
    };

    for (int e = 0; e < ECS_MAX_ENTITIES; ++e) {
        if (!ecs_alive_idx(e)) continue;
        if ((ecs_mask[e] & (CMP_FOLLOW | CMP_POS | CMP_VEL)) != (CMP_FOLLOW | CMP_POS | CMP_VEL)) continue;

        if ((ecs_mask[e] & CMP_LIFTABLE) &&
            cmp_liftable[e].state != LIFTABLE_STATE_ONGROUND)
        {
            cmp_vel[e].x = 0.0f;
            cmp_vel[e].y = 0.0f;
            continue;
        }

        cmp_follow_t* f = &cmp_follow[e];
        int target_idx = ent_index_checked(f->target);

        if (target_idx < 0 || (ecs_mask[target_idx] & CMP_POS) == 0) {
            cmp_vel[e].x = 0.0f;
            cmp_vel[e].y = 0.0f;
            continue;
        }

        float follower_x = cmp_pos[e].x;
        float follower_y = cmp_pos[e].y;
        float target_x = cmp_pos[target_idx].x;
        float target_y = cmp_pos[target_idx].y;
        float clear_hx = 0.0f;
        float clear_hy = 0.0f;
        if (ecs_mask[e] & CMP_COL) {
            clear_hx = cmp_col[e].hx + 1.0f;
            clear_hy = cmp_col[e].hy + 1.0f;
        } else {
            clear_hx = clear_hy = (subtile > 0) ? (float)subtile * 0.5f : 4.0f;
        }

        bool can_see = world_has_line_of_sight(follower_x, follower_y, target_x, target_y, f->vision_range, clear_hx, clear_hy);
        if (can_see) {
            f->last_seen_x = target_x;
            f->last_seen_y = target_y;
            f->has_last_seen = true;
        }

        float goal_x = 0.0f, goal_y = 0.0f, stop_radius = 0.0f;
        if (can_see) {
            goal_x = target_x;
            goal_y = target_y;
            stop_radius = f->desired_distance;
        } else if (f->has_last_seen) {
            goal_x = f->last_seen_x;
            goal_y = f->last_seen_y;
            stop_radius = stop_at_last_seen;
        } else {
            cmp_vel[e].x = 0.0f;
            cmp_vel[e].y = 0.0f;
            continue;
        }

        float dx = goal_x - follower_x;
        float dy = goal_y - follower_y;
        float dist2 = dx * dx + dy * dy;
        if (dist2 <= stop_radius * stop_radius) {
            cmp_vel[e].x = 0.0f;
            cmp_vel[e].y = 0.0f;
            continue;
        }

        float dist = sqrtf(dist2);
        if (dist <= 0.0f || f->max_speed <= 0.0f) {
            cmp_vel[e].x = 0.0f;
            cmp_vel[e].y = 0.0f;
            continue;
        }

        float dirx = dx / dist;
        float diry = dy / dist;

        float probe = (subtile > 0) ? (float)subtile * 1.5f : 12.0f;
        if (probe < clear_hx * 2.0f) probe = clear_hx * 2.0f;
        if (probe < clear_hy * 2.0f) probe = clear_hy * 2.0f;

        float chosen_dx = dirx;
        float chosen_dy = diry;
        bool found_clear = false;
        for (size_t ai = 0; ai < sizeof(angles)/sizeof(angles[0]); ++ai) {
            float ang = angles[ai];
            float s = sinf(ang);
            float c = cosf(ang);
            float rx = dirx * c - diry * s;
            float ry = dirx * s + diry * c;
            float end_x = follower_x + rx * probe;
            float end_y = follower_y + ry * probe;
            if (world_has_line_of_sight(follower_x, follower_y, end_x, end_y, probe, clear_hx, clear_hy)) {
                chosen_dx = rx;
                chosen_dy = ry;
                found_clear = true;
                break;
            }
        }

        if (!found_clear && !world_is_walkable_rect_px(follower_x, follower_y, clear_hx, clear_hy)) {
            cmp_vel[e].x = 0.0f;
            cmp_vel[e].y = 0.0f;
            continue;
        }

        cmp_vel[e].x = chosen_dx * f->max_speed;
        cmp_vel[e].y = chosen_dy * f->max_speed;
    }
}
