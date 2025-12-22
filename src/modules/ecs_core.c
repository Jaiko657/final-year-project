#include "../includes/ecs_internal.h"
#include "../includes/logger.h"
#include "../includes/world.h"
#include "../includes/renderer.h"
#include "../includes/dynarray.h"
#include <math.h>
#include <string.h>

// =============== ECS Storage =============
uint32_t        ecs_mask[ECS_MAX_ENTITIES];
uint32_t        ecs_gen[ECS_MAX_ENTITIES];
uint32_t        ecs_next_gen[ECS_MAX_ENTITIES];
cmp_position_t  cmp_pos[ECS_MAX_ENTITIES];
cmp_velocity_t  cmp_vel[ECS_MAX_ENTITIES];
cmp_follow_t    cmp_follow[ECS_MAX_ENTITIES];
cmp_anim_t      cmp_anim[ECS_MAX_ENTITIES];
cmp_sprite_t    cmp_spr[ECS_MAX_ENTITIES];
cmp_collider_t  cmp_col[ECS_MAX_ENTITIES];
cmp_trigger_t   cmp_trigger[ECS_MAX_ENTITIES];
cmp_billboard_t cmp_billboard[ECS_MAX_ENTITIES];
cmp_phys_body_t cmp_phys_body[ECS_MAX_ENTITIES];
cmp_liftable_t  cmp_liftable[ECS_MAX_ENTITIES];
cmp_door_t      cmp_door[ECS_MAX_ENTITIES];

// ========== O(1) create/delete ==========
static int free_stack[ECS_MAX_ENTITIES];
static int free_top = 0;

// Config
static const float PHYS_DEFAULT_MASS        = 1.0f;
static const float LIFTABLE_DEFAULT_CARRY_HEIGHT      = 18.0f;
static const float LIFTABLE_DEFAULT_CARRY_DISTANCE    = 12.0f;
static const float LIFTABLE_DEFAULT_PICKUP_DISTANCE   = 18.0f;
static const float LIFTABLE_DEFAULT_PICKUP_RADIUS     = 10.0f;
static const float LIFTABLE_DEFAULT_THROW_SPEED       = 220.0f;
static const float LIFTABLE_DEFAULT_THROW_VERTICAL    = 260.0f;
static const float LIFTABLE_DEFAULT_GRAVITY           = -720.0f;
static const float LIFTABLE_DEFAULT_AIR_FRICTION      = 3.0f;
static const float LIFTABLE_DEFAULT_BOUNCE_DAMPING    = 0.45f;

// =============== Helpers ==================
int ent_index_checked(ecs_entity_t e) {
    return (e.idx < ECS_MAX_ENTITIES && ecs_gen[e.idx] == e.gen && e.gen != 0)
        ? (int)e.idx : -1;
}

int ent_index_unchecked(ecs_entity_t e){ return (int)e.idx; }

bool ecs_alive_idx(int i){ return ecs_gen[i] != 0; }

bool ecs_alive_handle(ecs_entity_t e){ return ent_index_checked(e) >= 0; }

ecs_entity_t handle_from_index(int i){
    return (ecs_entity_t){ (uint32_t)i, ecs_gen[i] };
}

float clampf(float v, float a, float b){
    return (v < a) ? a : ((v > b) ? b : v);
}

static void set_position_sync_body(int i, float x, float y){
    cmp_pos[i] = (cmp_position_t){ x, y };
}

ecs_entity_t find_player_handle(void){
    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (ecs_alive_idx(i) && (ecs_mask[i] & CMP_PLAYER)) {
            return (ecs_entity_t){ (uint32_t)i, ecs_gen[i] };
        }
    }
    return ecs_null();
}

static void cmp_sprite_release_idx(int i){
    if (!(ecs_mask[i] & CMP_SPR)) return;
    if (asset_texture_valid(cmp_spr[i].tex)){
        asset_release_texture(cmp_spr[i].tex);
        cmp_spr[i].tex = (tex_handle_t){0,0};
    }
}

static void try_create_phys_body(int i){
    const uint32_t req = (CMP_POS | CMP_COL | CMP_PHYS_BODY);
    if ((ecs_mask[i] & req) != req) return;
    if (cmp_phys_body[i].created) return;
    ecs_phys_body_create_for_entity(i);
}

static void resolve_tile_penetration(int i)
{
    const uint32_t req = (CMP_POS | CMP_COL | CMP_PHYS_BODY);
    if ((ecs_mask[i] & req) != req) return;
    if (!cmp_phys_body[i].created) return;
    if ((ecs_mask[i] & CMP_LIFTABLE) && cmp_liftable[i].state != LIFTABLE_STATE_ONGROUND) return;

    int tiles_w = 0, tiles_h = 0;
    world_size_tiles(&tiles_w, &tiles_h);
    int tile_px = world_tile_size();
    int subtile_px = world_subtile_size();
    if (tiles_w <= 0 || tiles_h <= 0 || tile_px <= 0 || subtile_px <= 0) return;
    int subtiles_per_tile = tile_px / subtile_px;
    if (subtiles_per_tile <= 0) return;
    int subtiles_w = tiles_w * subtiles_per_tile;
    int subtiles_h = tiles_h * subtiles_per_tile;

    float hx = cmp_col[i].hx;
    float hy = cmp_col[i].hy;
    if (hx <= 0.0f || hy <= 0.0f) return;

    float cx = cmp_pos[i].x;
    float cy = cmp_pos[i].y;
    float left = cx - hx;
    float right = cx + hx;
    float bottom = cy - hy;
    float top = cy + hy;

    int min_sx = (int)floorf(left / (float)subtile_px);
    int max_sx = (int)floorf(right / (float)subtile_px);
    int min_sy = (int)floorf(bottom / (float)subtile_px);
    int max_sy = (int)floorf(top / (float)subtile_px);

    if (min_sx < 0) min_sx = 0;
    if (min_sy < 0) min_sy = 0;
    if (max_sx >= subtiles_w) max_sx = subtiles_w - 1;
    if (max_sy >= subtiles_h) max_sy = subtiles_h - 1;

    bool moved = false;
    for (int sy = min_sy; sy <= max_sy; ++sy) {
        for (int sx = min_sx; sx <= max_sx; ++sx) {
            if (world_is_walkable_subtile(sx, sy)) continue;

            float tile_left = (float)sx * (float)subtile_px;
            float tile_right = tile_left + (float)subtile_px;
            float tile_bottom = (float)sy * (float)subtile_px;
            float tile_top = tile_bottom + (float)subtile_px;

            if (right <= tile_left || left >= tile_right) continue;
            if (top <= tile_bottom || bottom >= tile_top) continue;

            float pen_x_left = right - tile_left;
            float pen_x_right = tile_right - left;
            float pen_y_bottom = top - tile_bottom;
            float pen_y_top = tile_top - bottom;

            float resolve_x = (pen_x_left < pen_x_right) ? -pen_x_left : pen_x_right;
            float resolve_y = (pen_y_bottom < pen_y_top) ? -pen_y_bottom : pen_y_top;

            if (fabsf(resolve_x) < fabsf(resolve_y)) {
                cx += resolve_x;
                left += resolve_x;
                right += resolve_x;
            } else {
                cy += resolve_y;
                bottom += resolve_y;
                top += resolve_y;
            }
            moved = true;
        }
    }

    if (moved) {
        set_position_sync_body(i, cx, cy);
    }
}

// forward
static void ecs_register_builtin_systems(void);

// =============== Public: lifecycle ========
void ecs_init(void){
    memset(ecs_mask, 0, sizeof(ecs_mask));
    memset(ecs_gen,  0, sizeof(ecs_gen));
    memset(ecs_next_gen, 0, sizeof(ecs_next_gen));
    free_top = 0;
    ecs_anim_reset_allocator();
    for (int i = ECS_MAX_ENTITIES - 1; i >= 0; --i) {
        free_stack[free_top++] = i;
    }

    ecs_systems_init();
    ecs_register_builtin_systems();
}

void ecs_shutdown(void){
    ecs_anim_shutdown_allocator();
}

bool ecs_get_player_position(float* out_x, float* out_y){
    ecs_entity_t player = find_player_handle();
    int idx = ent_index_checked(player);
    if (idx < 0 || !(ecs_mask[idx] & CMP_POS)) return false;

    if (out_x) *out_x = cmp_pos[idx].x;
    if (out_y) *out_y = cmp_pos[idx].y;
    return true;
}

bool ecs_get_position(ecs_entity_t e, v2f* out_pos){
    int idx = ent_index_checked(e);
    if (idx < 0 || !(ecs_mask[idx] & CMP_POS)) return false;
    if (out_pos) {
        *out_pos = v2f_make(cmp_pos[idx].x, cmp_pos[idx].y);
    }
    return true;
}

ecs_entity_t ecs_find_player(void){
    return find_player_handle();
}

// =============== Public: entity ===========
ecs_entity_t ecs_create(void)
{
    if (free_top == 0) return ecs_null();
    int idx = free_stack[--free_top];
    uint32_t g = ecs_next_gen[idx];
    if (g == 0) g = 1;
    ecs_gen[idx] = g;
    ecs_mask[idx] = 0;
    return (ecs_entity_t){ (uint32_t)idx, g };
}

void ecs_destroy(ecs_entity_t e)
{
    int idx = ent_index_checked(e);
    if (idx < 0) return;

    cmp_sprite_release_idx(idx);
    if (ecs_mask[idx] & CMP_PHYS_BODY) {
        ecs_phys_body_destroy_for_entity(idx);
        cmp_phys_body[idx].created = false;
    }
    if (ecs_mask[idx] & CMP_DOOR) {
        DA_FREE(&cmp_door[idx].tiles);
    }

    uint32_t g = ecs_gen[idx];
    g = (g + 1) ? (g + 1) : 1;
    ecs_gen[idx] = 0;
    ecs_next_gen[idx] = g;
    ecs_mask[idx] = 0;
    free_stack[free_top++] = idx;
}

// =============== Public: adders ===========
void cmp_add_position(ecs_entity_t e, float x, float y)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    ecs_mask[i] |= CMP_POS;
    set_position_sync_body(i, x, y);
    try_create_phys_body(i);
}

void cmp_add_velocity(ecs_entity_t e, float x, float y, facing_t direction)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    smoothed_facing_t smoothed_dir = {
        .rawDir = direction,
        .facingDir = direction,
        .candidateDir = direction,
        .candidateTime = 0.0f
    };
    cmp_vel[i] = (cmp_velocity_t){ x, y, smoothed_dir };
    ecs_mask[i] |= CMP_VEL;
}

void cmp_add_sprite_handle(ecs_entity_t e, tex_handle_t h, rectf src, float ox, float oy)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    if (asset_texture_valid(h)) asset_addref_texture(h);
    cmp_spr[i] = (cmp_sprite_t){ h, src, ox, oy };
    ecs_mask[i] |= CMP_SPR;
}

void cmp_add_sprite_path(ecs_entity_t e, const char* path, rectf src, float ox, float oy)
{
    tex_handle_t h = asset_acquire_texture(path);
    cmp_add_sprite_handle(e, h, src, ox, oy);
    asset_release_texture(h);
}

void cmp_add_player(ecs_entity_t e)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    ecs_mask[i] |= CMP_PLAYER;
}

void cmp_add_follow(ecs_entity_t e, ecs_entity_t target, float desired_distance, float max_speed)
{
    int i = ent_index_checked(e);
    if (i < 0) return;

    cmp_follow_t f = {
        .target           = target,
        .desired_distance = desired_distance,
        .max_speed        = max_speed,
        .vision_range     = -1.0f,
        .last_seen_x      = 0.0f,
        .last_seen_y      = 0.0f,
        .has_last_seen    = false
    };

    int t_idx = ent_index_checked(target);
    if (t_idx >= 0 && (ecs_mask[t_idx] & CMP_POS)) {
        f.last_seen_x   = cmp_pos[t_idx].x;
        f.last_seen_y   = cmp_pos[t_idx].y;
        f.has_last_seen = true;
    }

    cmp_follow[i] = f;
    ecs_mask[i] |= CMP_FOLLOW;
}

void cmp_add_trigger(ecs_entity_t e, float pad, uint32_t target_mask){
    int i = ent_index_checked(e); if (i < 0) return;
    cmp_trigger[i] = (cmp_trigger_t){ pad, target_mask };
    ecs_mask[i] |= CMP_TRIGGER;
}

void cmp_add_billboard(ecs_entity_t e, const char* text, float y_off, float linger, billboard_state_t state){
    int i = ent_index_checked(e); if (i<0) return;
    if ((ecs_mask[i]&(CMP_TRIGGER))!=(CMP_TRIGGER)){
        LOGC(LOGCAT_ECS, LOG_LVL_WARN, "Billboard added to entity without trigger. ENTITY: %i, %i", e.idx, e.gen);
    }
    strncpy(cmp_billboard[i].text, text, sizeof(cmp_billboard[i].text)-1);
    cmp_billboard[i].text[sizeof(cmp_billboard[i].text)-1] = '\0';
    cmp_billboard[i].y_offset = y_off;
    cmp_billboard[i].linger   = linger;
    cmp_billboard[i].timer    = 0.0f;
    cmp_billboard[i].state    = state;
    ecs_mask[i] |= CMP_BILLBOARD;
}

void cmp_add_size(ecs_entity_t e, float hx, float hy)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    cmp_col[i] = (cmp_collider_t){ hx, hy };
    ecs_mask[i] |= CMP_COL;
    try_create_phys_body(i);
}

void cmp_add_liftable(ecs_entity_t e)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    cmp_liftable[i] = (cmp_liftable_t){
        .state              = LIFTABLE_STATE_ONGROUND,
        .carrier            = ecs_null(),
        .height             = 0.0f,
        .vertical_velocity  = 0.0f,
        .carry_height       = LIFTABLE_DEFAULT_CARRY_HEIGHT,
        .carry_distance     = LIFTABLE_DEFAULT_CARRY_DISTANCE,
        .pickup_distance    = LIFTABLE_DEFAULT_PICKUP_DISTANCE,
        .pickup_radius      = LIFTABLE_DEFAULT_PICKUP_RADIUS,
        .throw_speed        = LIFTABLE_DEFAULT_THROW_SPEED,
        .throw_vertical_speed = LIFTABLE_DEFAULT_THROW_VERTICAL,
        .gravity            = LIFTABLE_DEFAULT_GRAVITY,
        .vx                 = 0.0f,
        .vy                 = 0.0f,
        .air_friction       = LIFTABLE_DEFAULT_AIR_FRICTION,
        .bounce_damping     = LIFTABLE_DEFAULT_BOUNCE_DAMPING,
    };
    ecs_mask[i] |= CMP_LIFTABLE;
}

void cmp_add_phys_body(ecs_entity_t e, PhysicsType type, float mass)
{
    int i = ent_index_checked(e);
    if (i < 0) return;

    float inv_mass = (mass != 0.0f) ? (1.0f / mass) : 0.0f;
    cmp_phys_body[i] = (cmp_phys_body_t){
        .type = type,
        .mass = mass,
        .inv_mass = inv_mass,
        .created = false
    };
    ecs_mask[i] |= CMP_PHYS_BODY;
    try_create_phys_body(i);
}

void cmp_add_phys_body_default(ecs_entity_t e, PhysicsType type)
{
    cmp_add_phys_body(e, type, PHYS_DEFAULT_MASS);
}

void cmp_add_door(ecs_entity_t e, float prox_radius, float prox_off_x, float prox_off_y, int tile_count, const door_tile_xy_t* tile_xy)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    cmp_door_t* d = &cmp_door[i];
    DA_FREE(&d->tiles);
    memset(d, 0, sizeof(*d));
    d->prox_radius = prox_radius;
    d->prox_off_x = prox_off_x;
    d->prox_off_y = prox_off_y;
    if (tile_xy && tile_count > 0) {
        DA_RESERVE(&d->tiles, tile_count);
        for (int t = 0; t < tile_count; ++t) {
            door_tile_info_t info = {
                .x = tile_xy[t].x,
                .y = tile_xy[t].y,
                .layer_idx = -1,
                .layer_name = {0},
                .tileset_idx = -1,
                .base_tile_id = 0,
                .flip_flags = 0
            };
            DA_APPEND(&d->tiles, info);
        }
    }
    d->state = DOOR_CLOSED;
    d->current_frame = 0;
    d->anim_time_ms = 0.0f;
    d->intent_open = false;
    d->resolved_map_gen = 0;
    ecs_mask[i] |= CMP_DOOR;
}

// =============== Systems (internal) ======
static facing_t dir_from_input(const input_t* in, facing_t fallback)
{
    if (in->moveX == 0.0f && in->moveY == 0.0f) {
        return fallback;
    }

    if (in->moveY < 0.0f) {                 // NORTH half
        if      (in->moveX < 0.0f) return DIR_NORTHWEST;
        else if (in->moveX > 0.0f) return DIR_NORTHEAST;
        else                       return DIR_NORTH;
    }
    else if (in->moveY > 0.0f) {           // SOUTH half
        if      (in->moveX < 0.0f) return DIR_SOUTHWEST;
        else if (in->moveX > 0.0f) return DIR_SOUTHEAST;
        else                       return DIR_SOUTH;
    }
    else {                                 // Horizontal only
        if      (in->moveX < 0.0f) return DIR_WEST;
        else if (in->moveX > 0.0f) return DIR_EAST;
    }
    //shuts up compiler, but not sure if its even possible to get here
    return fallback;
}

static void sys_input(float dt, const input_t* in)
{
    const float SPEED       = 120.0f;
    const float CHANGE_TIME = 0.04f;   // 40 ms

    for (int e = 0; e < ECS_MAX_ENTITIES; ++e) {
        if (!ecs_alive_idx(e)) continue;
        if ((ecs_mask[e] & (CMP_PLAYER | CMP_VEL)) != (CMP_PLAYER | CMP_VEL)) continue;

        cmp_velocity_t*      v = &cmp_vel[e];
        smoothed_facing_t*   f = &v->facing;

        v->x = in->moveX * SPEED;
        v->y = in->moveY * SPEED;

        const int hasInput = (in->moveX != 0.0f || in->moveY != 0.0f);
        if (!hasInput) {
            f->rawDir        = f->facingDir;
            f->candidateDir  = f->facingDir;
            f->candidateTime = 0.0f;
            break;
        }
        facing_t newRaw = dir_from_input(in, f->facingDir);
        f->rawDir = newRaw;
        if (newRaw == f->facingDir) {
            f->candidateDir  = f->facingDir;
            f->candidateTime = 0.0f;
            break;
        }

        // Add time
        if (newRaw == f->candidateDir) {
            f->candidateTime += dt;
        } else {
            f->candidateDir  = newRaw;
            f->candidateTime = dt;
        }

        // Threshold hit
        if (f->candidateTime >= CHANGE_TIME) {
            f->facingDir     = f->candidateDir;
            f->candidateTime = 0.0f;
        }
    }
}

static void sys_follow(float dt)
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

static void sys_physics_integrate_impl(float dt)
{
    if (!world_get_tiled_map()) return;

    // Ensure any newly-tagged entities participate in the physics-lite step.
    for (int e = 0; e < ECS_MAX_ENTITIES; ++e) {
        if (!ecs_alive_idx(e)) continue;
        const uint32_t req = (CMP_POS | CMP_COL | CMP_PHYS_BODY);
        if ((ecs_mask[e] & req) != req) continue;
        if (!cmp_phys_body[e].created) {
            ecs_phys_body_create_for_entity(e);
        }
    }

    bool  has_intent[ECS_MAX_ENTITIES];
    for (int e = 0; e < ECS_MAX_ENTITIES; ++e) {
        has_intent[e] = false;
    }

    // Apply intent velocities to positions (physics-lite).
    for (int e = 0; e < ECS_MAX_ENTITIES; ++e) {
        if (!ecs_alive_idx(e)) continue;
        const uint32_t req = (CMP_VEL | CMP_PHYS_BODY);
        if ((ecs_mask[e] & req) != req) continue;

        cmp_velocity_t*  v  = &cmp_vel[e];
        cmp_phys_body_t* pb = &cmp_phys_body[e];
        if (!pb->created) continue;
        if ((ecs_mask[e] & CMP_LIFTABLE) && cmp_liftable[e].state != LIFTABLE_STATE_ONGROUND) {
            // Liftables handle their own airborne motion and disable collisions while carried/thrown.
            v->x = 0.0f;
            v->y = 0.0f;
            continue;
        }

        switch (pb->type) {
            case PHYS_DYNAMIC:
            case PHYS_KINEMATIC:
                if (v->x != 0.0f || v->y != 0.0f) {
                    has_intent[e] = true;
                }
                cmp_pos[e].x += v->x * dt;
                cmp_pos[e].y += v->y * dt;
                break;
            default:
                break;
        }

        v->x = 0.0f;
        v->y = 0.0f;
    }

    // Resolve entity/entity overlaps with a small iterative solver.
    // Goal: preserve the current "buggy-but-good" feel:
    // - NPCs (usually no intent) only move while being pushed.
    // - The pusher (usually player) "struggles" and loses some of its intended motion.
    for (int iter = 0; iter < 4; ++iter) {
        for (int a = 0; a < ECS_MAX_ENTITIES; ++a) {
            if (!ecs_alive_idx(a)) continue;
            const uint32_t reqA = (CMP_POS | CMP_COL | CMP_PHYS_BODY);
            if ((ecs_mask[a] & reqA) != reqA) continue;
            if (!cmp_phys_body[a].created) continue;
            if ((ecs_mask[a] & CMP_LIFTABLE) && cmp_liftable[a].state != LIFTABLE_STATE_ONGROUND) continue;

            for (int b = a + 1; b < ECS_MAX_ENTITIES; ++b) {
                if (!ecs_alive_idx(b)) continue;
                const uint32_t reqB = (CMP_POS | CMP_COL | CMP_PHYS_BODY);
                if ((ecs_mask[b] & reqB) != reqB) continue;
                if (!cmp_phys_body[b].created) continue;
                if ((ecs_mask[b] & CMP_LIFTABLE) && cmp_liftable[b].state != LIFTABLE_STATE_ONGROUND) continue;

                const cmp_phys_body_t* pa = &cmp_phys_body[a];
                const cmp_phys_body_t* pb = &cmp_phys_body[b];

                // Optional collision filtering (only if configured on either body).
                if (pa->category_bits || pa->mask_bits || pb->category_bits || pb->mask_bits) {
                    const unsigned int catA = pa->category_bits ? pa->category_bits : 0xFFFFFFFFu;
                    const unsigned int mskA = pa->mask_bits ? pa->mask_bits : 0xFFFFFFFFu;
                    const unsigned int catB = pb->category_bits ? pb->category_bits : 0xFFFFFFFFu;
                    const unsigned int mskB = pb->mask_bits ? pb->mask_bits : 0xFFFFFFFFu;
                    if (((catA & mskB) == 0u) || ((catB & mskA) == 0u)) continue;
                }

                if (pa->type == PHYS_STATIC && pb->type == PHYS_STATIC) continue;

                const float ax = cmp_pos[a].x;
                const float ay = cmp_pos[a].y;
                const float bx = cmp_pos[b].x;
                const float by = cmp_pos[b].y;
                const float ahx = cmp_col[a].hx;
                const float ahy = cmp_col[a].hy;
                const float bhx = cmp_col[b].hx;
                const float bhy = cmp_col[b].hy;

                const float dx = bx - ax;
                const float dy = by - ay;
                const float px = (ahx + bhx) - fabsf(dx);
                const float py = (ahy + bhy) - fabsf(dy);
                if (px <= 0.0f || py <= 0.0f) continue;

                const bool resolve_x = (px < py);
                const float overlap = resolve_x ? px : py;
                const float sign = resolve_x ? (dx >= 0.0f ? 1.0f : -1.0f) : (dy >= 0.0f ? 1.0f : -1.0f);

                float wA = pa->inv_mass;
                float wB = pb->inv_mass;
                if (has_intent[a]) wA *= 2.0f;
                if (has_intent[b]) wB *= 2.0f;

                if (pa->type == PHYS_STATIC) wA = 0.0f;
                if (pb->type == PHYS_STATIC) wB = 0.0f;

                float sum = wA + wB;
                float a_amt = 0.0f;
                float b_amt = 0.0f;
                if (sum > 0.0f) {
                    a_amt = overlap * (wA / sum);
                    b_amt = overlap * (wB / sum);
                } else {
                    // Fallback: split evenly.
                    a_amt = overlap * 0.5f;
                    b_amt = overlap * 0.5f;
                }

                // Separate along chosen axis.
                if (resolve_x) {
                    cmp_pos[a].x -= sign * a_amt;
                    cmp_pos[b].x += sign * b_amt;
                } else {
                    cmp_pos[a].y -= sign * a_amt;
                    cmp_pos[b].y += sign * b_amt;
                }
            }
        }

        for (int e = 0; e < ECS_MAX_ENTITIES; ++e) {
            if (!ecs_alive_idx(e)) continue;
            resolve_tile_penetration(e);
        }
    }
}

// ========= Public: update/iterators ============
void ecs_tick(float dt, const input_t* in)
{
    ecs_run_phase(PHASE_INPUT,       dt, in);
    ecs_run_phase(PHASE_SIM_PRE,     dt, in);
    ecs_run_phase(PHASE_PHYSICS,     dt, in);
    ecs_run_phase(PHASE_SIM_POST,    dt, in);
    ecs_run_phase(PHASE_DEBUG,       dt, in);
}

void ecs_present(float frame_dt)
{
    ecs_run_phase(PHASE_PRESENT, frame_dt, NULL);
}

ecs_count_result_t ecs_count_entities(const uint32_t* masks, int num_masks)
{
    ecs_count_result_t result = { .num = num_masks };
    memset(result.count, 0, sizeof(result.count));

    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        uint32_t mask = ecs_mask[i];
        for (int j = 0; j < num_masks; ++j) {
            if ((mask & masks[j]) == masks[j]) {
                result.count[j]++;
            }
        }
    }
    return result;
}

// =============== Adapters for system registry =========
static void sys_input_adapt(float dt, const input_t* in) { sys_input(dt, in); }
static void sys_follow_adapt(float dt, const input_t* in) { (void)in; sys_follow(dt); }
static void sys_physics_adapt(float dt, const input_t* in) { (void)in; sys_physics_integrate_impl(dt); }
static void sys_world_apply_edits_adapt(float dt, const input_t* in) { (void)dt; (void)in; world_apply_tile_edits(); }
#if DEBUG_BUILD
void sys_debug_binds(const input_t* in);
static void sys_debug_binds_adapt(float dt, const input_t* in) { (void)dt; sys_debug_binds(in); }
#endif

// TODO: dont know if i like forward declaring like this

// from ecs_anim.c
void sys_anim_sprite_adapt(float dt, const input_t* in);
void sys_anim_controller_adapt(float dt, const input_t* in);

// from ecs_proximity.c
void sys_prox_build_adapt(float dt, const input_t* in);
void sys_billboards_adapt(float dt, const input_t* in);

// from ecs_liftable.c
void sys_liftable_input_adapt(float dt, const input_t* in);
void sys_liftable_motion_adapt(float dt, const input_t* in);

// =============== Registration =========
static void ecs_register_builtin_systems(void)
{
    ecs_register_system(PHASE_INPUT,    0,   sys_input_adapt, "input");
    ecs_register_system(PHASE_INPUT,    50,  sys_liftable_input_adapt, "liftable_input");

    ecs_register_system(PHASE_PHYSICS,  50,  sys_follow_adapt,  "follow_ai");
    ecs_register_system(PHASE_PHYSICS,  90,  sys_liftable_motion_adapt, "liftable_motion");
    ecs_register_system(PHASE_SIM_PRE,  200, sys_anim_controller_adapt, "animation_controller");

    ecs_register_system(PHASE_PHYSICS,  100, sys_physics_adapt, "physics");

    ecs_register_system(PHASE_SIM_POST, 100, sys_prox_build_adapt,      "proximity_view");
    ecs_register_system(PHASE_SIM_POST, 200, sys_billboards_adapt, "billboards");

    ecs_register_system(PHASE_PRESENT,  100, sys_anim_sprite_adapt,"sprite_anim");
    ecs_register_system(PHASE_PRESENT,  900, sys_world_apply_edits_adapt, "world_apply_edits");

#if DEBUG_BUILD
    ecs_register_system(PHASE_DEBUG,    100, sys_debug_binds_adapt, "debug_binds");
#endif
}
