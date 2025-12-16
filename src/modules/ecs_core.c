#include "../includes/ecs_internal.h"
#include "../includes/logger.h"
#include "../includes/world.h"
#include "../includes/world_physics.h"
#include "../includes/renderer.h"
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
static const float PHYS_DEFAULT_RESTITUTION = 0.0f;
static const float PHYS_DEFAULT_FRICTION    = 0.8f;
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
    if ((ecs_mask[i] & CMP_PHYS_BODY) && cmp_phys_body[i].cp_body) {
        cpBodySetPosition(cmp_phys_body[i].cp_body, cpv(x, y));
    }
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
    if (cmp_phys_body[i].cp_body || cmp_phys_body[i].cp_shape) return;
    ecs_phys_body_create_for_entity(i);
}

static void resolve_tile_penetration(int i)
{
    const uint32_t req = (CMP_POS | CMP_COL | CMP_PHYS_BODY);
    if ((ecs_mask[i] & req) != req) return;
    if (!cmp_phys_body[i].cp_body) return;
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
    for (int i = ECS_MAX_ENTITIES - 1; i >= 0; --i) {
        free_stack[free_top++] = i;
    }

    ecs_systems_init();
    ecs_register_builtin_systems();
}

void ecs_shutdown(void){
    // no heap to free currently
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
        cmp_phys_body[idx].cp_body = NULL;
        cmp_phys_body[idx].cp_shape = NULL;
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

    cmp_follow[i] = (cmp_follow_t){ target, desired_distance, max_speed };
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

void cmp_add_phys_body(ecs_entity_t e, PhysicsType type, float mass, float restitution, float friction)
{
    int i = ent_index_checked(e);
    if (i < 0) return;

    float inv_mass = (mass != 0.0f) ? (1.0f / mass) : 0.0f;
    cmp_phys_body[i] = (cmp_phys_body_t){
        .type = type,
        .mass = mass,
        .inv_mass = inv_mass,
        .restitution = restitution,
        .friction = friction,
        .cp_body = NULL,
        .cp_shape = NULL
    };
    ecs_mask[i] |= CMP_PHYS_BODY;
    try_create_phys_body(i);
}

void cmp_add_phys_body_default(ecs_entity_t e, PhysicsType type)
{
    cmp_add_phys_body(e, type, PHYS_DEFAULT_MASS, PHYS_DEFAULT_RESTITUTION, PHYS_DEFAULT_FRICTION);
}

void cmp_add_door(ecs_entity_t e, float prox_radius, float prox_off_x, float prox_off_y, int tile_count, const int (*tile_xy)[2])
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    memset(&cmp_door[i], 0, sizeof(cmp_door[i]));
    cmp_door[i].prox_radius = prox_radius;
    cmp_door[i].prox_off_x = prox_off_x;
    cmp_door[i].prox_off_y = prox_off_y;
    if (tile_xy && tile_count > 0) {
        if (tile_count > 4) tile_count = 4;
        cmp_door[i].tile_count = tile_count;
        for (int t = 0; t < tile_count; ++t) {
            cmp_door[i].tiles[t].x = tile_xy[t][0];
            cmp_door[i].tiles[t].y = tile_xy[t][1];
        }
    }
    cmp_door[i].state = DOOR_CLOSED;
    cmp_door[i].current_frame = 0;
    cmp_door[i].anim_time_ms = 0.0f;
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

        float dx = cmp_pos[target_idx].x - cmp_pos[e].x;
        float dy = cmp_pos[target_idx].y - cmp_pos[e].y;
        float desired = f->desired_distance;

        float dist2 = dx * dx + dy * dy;
        if (dist2 <= desired * desired) {
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

        cmp_vel[e].x = dirx * f->max_speed;
        cmp_vel[e].y = diry * f->max_speed;
    }
}

static void sys_physics_integrate_impl(float dt)
{
    if (!world_physics_ready()) return;

    // Ensure any newly-tagged entities create their Chipmunk bodies.
    for (int e = 0; e < ECS_MAX_ENTITIES; ++e) {
        if (!ecs_alive_idx(e)) continue;
        const uint32_t req = (CMP_POS | CMP_COL | CMP_PHYS_BODY);
        if ((ecs_mask[e] & req) != req) continue;
        if (!cmp_phys_body[e].cp_body && !cmp_phys_body[e].cp_shape) {
            ecs_phys_body_create_for_entity(e);
        }
    }

    // Apply intent velocities to Chipmunk bodies.
    for (int e = 0; e < ECS_MAX_ENTITIES; ++e) {
        if (!ecs_alive_idx(e)) continue;
        const uint32_t req = (CMP_VEL | CMP_PHYS_BODY);
        if ((ecs_mask[e] & req) != req) continue;

        cmp_velocity_t*  v  = &cmp_vel[e];
        cmp_phys_body_t* pb = &cmp_phys_body[e];
        if (!pb->cp_body) continue;

        switch (pb->type) {
            case PHYS_DYNAMIC:
            case PHYS_KINEMATIC:
                cpBodySetVelocity(pb->cp_body, cpv(v->x, v->y));
                break;
            default:
                break;
        }

        v->x = 0.0f;
        v->y = 0.0f;
    }

    world_physics_step(dt);

    // Sync Chipmunk positions back to ECS.
    for (int e = 0; e < ECS_MAX_ENTITIES; ++e) {
        if (!ecs_alive_idx(e)) continue;
        const uint32_t req = (CMP_POS | CMP_PHYS_BODY);
        if ((ecs_mask[e] & req) != req) continue;

        cmp_phys_body_t* pb = &cmp_phys_body[e];
        if (!pb->cp_body) continue;

        cpVect p = cpBodyGetPosition(pb->cp_body);
        cmp_pos[e].x = p.x;
        cmp_pos[e].y = p.y;
    }

    for (int e = 0; e < ECS_MAX_ENTITIES; ++e) {
        if (!ecs_alive_idx(e)) continue;
        resolve_tile_penetration(e);
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

#if DEBUG_BUILD
    ecs_register_system(PHASE_DEBUG,    100, sys_debug_binds_adapt, "debug_binds");
#endif
}
