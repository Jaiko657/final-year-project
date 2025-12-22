#include "../includes/ecs_internal.h"
#include "../includes/input.h"
#include "../includes/world.h"
#include <float.h>
#include <math.h>

static const float DIAG = 0.70710678f;

static v2f facing_to_dir(facing_t dir)
{
    switch (dir) {
        case DIR_NORTH:     return v2f_make(0.0f, -1.0f);
        case DIR_NORTHEAST: return v2f_make( DIAG, -DIAG);
        case DIR_EAST:      return v2f_make(1.0f, 0.0f);
        case DIR_SOUTHEAST: return v2f_make( DIAG, DIAG);
        case DIR_SOUTH:     return v2f_make(0.0f, 1.0f);
        case DIR_SOUTHWEST: return v2f_make(-DIAG, DIAG);
        case DIR_WEST:      return v2f_make(-1.0f, 0.0f);
        case DIR_NORTHWEST: return v2f_make(-DIAG, -DIAG);
        default:            return v2f_make(0.0f, 1.0f);
    }
}

static v2f player_facing_dir(int idx)
{
    if (idx < 0 || idx >= ECS_MAX_ENTITIES) return v2f_make(0.0f, 1.0f);
    if ((ecs_mask[idx] & CMP_VEL) == 0) return v2f_make(0.0f, 1.0f);
    return facing_to_dir(cmp_vel[idx].facing.facingDir);
}

static void liftable_move_entity(int idx, float x, float y)
{
    if (idx < 0 || idx >= ECS_MAX_ENTITIES) return;
    cmp_pos[idx] = (cmp_position_t){ x, y };
}

static void liftable_set_state(int idx, liftable_state_t new_state)
{
    cmp_liftable_t* l = &cmp_liftable[idx];

    if (new_state == LIFTABLE_STATE_ONGROUND) {
        l->carrier = ecs_null();
        l->height = 0.0f;
        l->vertical_velocity = 0.0f;
        l->vx = 0.0f;
        l->vy = 0.0f;
    }
    l->state = new_state;
}

static int find_carried_index(ecs_entity_t carrier)
{
    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & CMP_LIFTABLE) == 0) continue;
        cmp_liftable_t* l = &cmp_liftable[i];
        if (l->state != LIFTABLE_STATE_CARRIED) continue;
        if (l->carrier.idx == carrier.idx && l->carrier.gen == carrier.gen) {
            return i;
        }
    }
    return -1;
}

static int find_pickup_candidate(int player_idx, v2f dir)
{
    float px = cmp_pos[player_idx].x;
    float py = cmp_pos[player_idx].y;

    float best_facing = -2.0f;
    float best_dist2 = FLT_MAX;
    int best_idx = -1;

    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (i == player_idx) continue;
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & (CMP_LIFTABLE | CMP_POS)) != (CMP_LIFTABLE | CMP_POS)) continue;

        cmp_liftable_t* l = &cmp_liftable[i];
        if (l->state != LIFTABLE_STATE_ONGROUND) continue;

        float reach = l->pickup_distance > 0.0f ? l->pickup_distance : 12.0f;
        float radius = l->pickup_radius   > 0.0f ? l->pickup_radius   : 8.0f;

        float target_x = px + dir.x * reach;
        float target_y = py + dir.y * reach;
        float focus_dx = cmp_pos[i].x - target_x;
        float focus_dy = cmp_pos[i].y - target_y;
        float focus_d2 = focus_dx*focus_dx + focus_dy*focus_dy;
        float max_d2 = radius * radius;
        if (focus_d2 > max_d2) continue;

        float dir_dx = cmp_pos[i].x - px;
        float dir_dy = cmp_pos[i].y - py;
        float dist2 = dir_dx*dir_dx + dir_dy*dir_dy;
        float dist = sqrtf(dist2);
        float facing = 1.0f;
        if (dist > 0.0001f) {
            facing = (dir_dx / dist) * dir.x + (dir_dy / dist) * dir.y;
        }
        if (facing < 0.0f) continue;

        if (facing > best_facing + 1e-4f ||
            (fabsf(facing - best_facing) <= 1e-4f && dist2 < best_dist2))
        {
            best_facing = facing;
            best_dist2 = dist2;
            best_idx = i;
        }
    }
    return best_idx;
}

static void begin_carry(int idx, int player_idx, ecs_entity_t player_handle)
{
    if (idx < 0 || player_idx < 0) return;
    cmp_liftable_t* l = &cmp_liftable[idx];
    l->carrier = player_handle;
    l->height = l->carry_height;
    l->vertical_velocity = 0.0f;
    l->vx = 0.0f;
    l->vy = 0.0f;
    liftable_set_state(idx, LIFTABLE_STATE_CARRIED);

    v2f dir = player_facing_dir(player_idx);
    float base_x = cmp_pos[player_idx].x + dir.x * l->carry_distance;
    float base_y = cmp_pos[player_idx].y + dir.y * l->carry_distance;
    liftable_move_entity(idx, base_x, base_y);
}

static void throw_entity(int idx, int player_idx, v2f dir)
{
    if (idx < 0) return;
    cmp_liftable_t* l = &cmp_liftable[idx];
    if (dir.x == 0.0f && dir.y == 0.0f) {
        dir = v2f_make(0.0f, 1.0f);
    }
    l->carrier = ecs_null();
    l->height = l->carry_height;
    l->vertical_velocity = l->throw_vertical_speed;
    l->vx = dir.x * l->throw_speed;
    l->vy = dir.y * l->throw_speed;
    liftable_set_state(idx, LIFTABLE_STATE_THROWN);

    if (player_idx >= 0 && player_idx < ECS_MAX_ENTITIES) {
        float base_x = cmp_pos[player_idx].x + dir.x * l->carry_distance;
        float base_y = cmp_pos[player_idx].y + dir.y * l->carry_distance;
        liftable_move_entity(idx, base_x, base_y);
    }
}

static bool liftable_hits_tiles(float cx, float cy, float hx, float hy)
{
    float left = cx - hx;
    float right = cx + hx;
    float bottom = cy - hy;
    float top = cy + hy;

    int world_w = 0, world_h = 0;
    world_size_px(&world_w, &world_h);
    if (world_w > 0 && world_h > 0) {
        if (left < 0.0f || right > (float)world_w ||
            bottom < 0.0f || top > (float)world_h)
        {
            return true;
        }
    }

    int ss = world_subtile_size();
    if (ss <= 0) return false;

    int min_sx = (int)floorf(left / (float)ss);
    int max_sx = (int)floorf((right - 0.0001f) / (float)ss);
    int min_sy = (int)floorf(bottom / (float)ss);
    int max_sy = (int)floorf((top - 0.0001f) / (float)ss);

    for (int sy = min_sy; sy <= max_sy; ++sy) {
        for (int sx = min_sx; sx <= max_sx; ++sx) {
            if (!world_is_walkable_subtile(sx, sy)) {
                return true;
            }
        }
    }
    return false;
}

static void sys_liftable_input_impl(const input_t* in)
{
    if (!in || !input_pressed(in, BTN_LIFT)) return;

    ecs_entity_t player = find_player_handle();
    int player_idx = ent_index_checked(player);
    if (player_idx < 0 || !(ecs_mask[player_idx] & CMP_POS)) return;

    int carried_idx = find_carried_index(player);
    if (carried_idx >= 0) {
        v2f dir = player_facing_dir(player_idx);
        throw_entity(carried_idx, player_idx, dir);
        return;
    }

    v2f dir = player_facing_dir(player_idx);
    int pickup_idx = find_pickup_candidate(player_idx, dir);
    if (pickup_idx >= 0) {
        begin_carry(pickup_idx, player_idx, player);
    }
}

static void update_carried(int idx, cmp_liftable_t* l)
{
    int carrier_idx = ent_index_checked(l->carrier);
    if (carrier_idx < 0 ||
        !ecs_alive_idx(carrier_idx) ||
        (ecs_mask[carrier_idx] & CMP_POS) == 0)
    {
        liftable_set_state(idx, LIFTABLE_STATE_ONGROUND);
        return;
    }

    v2f dir = player_facing_dir(carrier_idx);
    float cx = cmp_pos[carrier_idx].x + dir.x * l->carry_distance;
    float cy = cmp_pos[carrier_idx].y + dir.y * l->carry_distance;
    liftable_move_entity(idx, cx, cy);
    l->height = l->carry_height;
    l->vertical_velocity = 0.0f;
}

static void update_thrown(int idx, cmp_liftable_t* l, float dt)
{
    float friction = clampf(l->air_friction * dt, 0.0f, 1.0f);
    float damping = 1.0f - friction;
    if (damping < 0.0f) damping = 0.0f;
    l->vx *= damping;
    l->vy *= damping;

    float hx = 0.0f, hy = 0.0f;
    if (ecs_mask[idx] & CMP_COL) {
        hx = cmp_col[idx].hx;
        hy = cmp_col[idx].hy;
    }

    float curr_x = cmp_pos[idx].x;
    float curr_y = cmp_pos[idx].y;

    float next_x = curr_x + l->vx * dt;
    if (liftable_hits_tiles(next_x, curr_y, hx, hy)) {
        l->vx = -l->vx * l->bounce_damping;
        if (fabsf(l->vx) < 5.0f) l->vx = 0.0f;
    } else {
        curr_x = next_x;
    }

    float next_y = curr_y + l->vy * dt;
    if (liftable_hits_tiles(curr_x, next_y, hx, hy)) {
        l->vy = -l->vy * l->bounce_damping;
        if (fabsf(l->vy) < 5.0f) l->vy = 0.0f;
    } else {
        curr_y = next_y;
    }

    liftable_move_entity(idx, curr_x, curr_y);

    l->vertical_velocity += l->gravity * dt;
    l->height += l->vertical_velocity * dt;

    if (l->height <= 0.0f) {
        l->height = 0.0f;
        liftable_set_state(idx, LIFTABLE_STATE_ONGROUND);
    }
}

static void sys_liftable_motion_impl(float dt)
{
    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & CMP_LIFTABLE) == 0) continue;

        cmp_liftable_t* l = &cmp_liftable[i];
        switch (l->state) {
            case LIFTABLE_STATE_CARRIED:
                update_carried(i, l);
                break;
            case LIFTABLE_STATE_THROWN:
                update_thrown(i, l, dt);
                break;
            default:
                break;
        }
    }
}

void sys_liftable_input_adapt(float dt, const input_t* in)
{
    (void)dt;
    sys_liftable_input_impl(in);
}

void sys_liftable_motion_adapt(float dt, const input_t* in)
{
    (void)in;
    sys_liftable_motion_impl(dt);
}
