#include "../includes/ecs_internal.h"
#include "../includes/logger.h"
#include <math.h>
#include <string.h>

// =============== ECS Storage =============
uint32_t        ecs_mask[ECS_MAX_ENTITIES];
uint32_t        ecs_gen[ECS_MAX_ENTITIES];
uint32_t        ecs_next_gen[ECS_MAX_ENTITIES];
cmp_position_t  cmp_pos[ECS_MAX_ENTITIES];
cmp_velocity_t  cmp_vel[ECS_MAX_ENTITIES];
cmp_anim_t      cmp_anim[ECS_MAX_ENTITIES];
cmp_sprite_t    cmp_spr[ECS_MAX_ENTITIES];
cmp_collider_t  cmp_col[ECS_MAX_ENTITIES];
cmp_trigger_t   cmp_trigger[ECS_MAX_ENTITIES];
cmp_billboard_t cmp_billboard[ECS_MAX_ENTITIES];

// ========== O(1) create/delete ==========
static int free_stack[ECS_MAX_ENTITIES];
static int free_top = 0;

// Config
static int g_worldW = 800;
static int g_worldH = 450;

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

void ecs_set_world_size(int w, int h){
    g_worldW = w;
    g_worldH = h;
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
    cmp_pos[i] = (cmp_position_t){ x, y };
    ecs_mask[i] |= CMP_POS;
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

void cmp_add_trigger(ecs_entity_t e, float pad, uint32_t target_mask){
    int i = ent_index_checked(e); if (i < 0) return;
    cmp_trigger[i] = (cmp_trigger_t){ pad, target_mask };
    ecs_mask[i] |= CMP_TRIGGER;
}

void cmp_add_billboard(ecs_entity_t e, const char* text, float y_off, float linger, billboard_state_t state){
    int i = ent_index_checked(e); if (i<0) return;
    if ((ecs_mask[i]&(CMP_TRIGGER))==(CMP_TRIGGER)){
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
}

bool ecs_get_position(ecs_entity_t e, v2f* out_pos)
{
    int i = ent_index_checked(e);
    if (i < 0) return false;
    if (!(ecs_mask[i] & CMP_POS)) return false;
    if (out_pos) {
        out_pos->x = cmp_pos[i].x;
        out_pos->y = cmp_pos[i].y;
    }
    return true;
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
    const float SPEED       = 200.0f;
    const float CHANGE_TIME = 0.08f;   // 80 ms

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

static void sys_physics(float dt)
{
    for (int e=0; e<ECS_MAX_ENTITIES; ++e){
        if(!ecs_alive_idx(e)) continue;
        if ((ecs_mask[e]&(CMP_POS|CMP_VEL))==(CMP_POS|CMP_VEL)){
            cmp_pos[e].x += cmp_vel[e].x * dt;
            cmp_pos[e].y += cmp_vel[e].y * dt;
        }
    }
}

static void sys_bounds(void)
{
    int w=g_worldW, h=g_worldH;
    for (int e=0; e<ECS_MAX_ENTITIES; ++e){
        if(!ecs_alive_idx(e)) continue;
        if (ecs_mask[e] & CMP_POS){
            if (ecs_mask[e] & CMP_COL){
                float hx = cmp_col[e].hx, hy = cmp_col[e].hy;
                cmp_pos[e].x = clampf(cmp_pos[e].x, hx, w - hx);
                cmp_pos[e].y = clampf(cmp_pos[e].y, hy, h - hy);
            } else {
                if(cmp_pos[e].x<0) cmp_pos[e].x=0;
                if(cmp_pos[e].y<0) cmp_pos[e].y=0;
                if(cmp_pos[e].x>w) cmp_pos[e].x=w;
                if(cmp_pos[e].y>h) cmp_pos[e].y=h;
            }
        }
    }
}

void sys_debug_binds(const input_t* in)
{
    if(input_pressed(in, BTN_ASSET_DEBUG_PRINT)) {
        asset_reload_all();
        asset_log_debug();
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
static void sys_physics_adapt(float dt, const input_t* in) { (void)in; sys_physics(dt); }
static void sys_bounds_adapt(float dt, const input_t* in) { (void)dt; (void)in; sys_bounds(); }
static void sys_debug_binds_adapt(float dt, const input_t* in) { (void)dt; sys_debug_binds(in); }

// TODO: dont know if i like forward declaring like this

// from ecs_anim.c
void sys_anim_sprite_adapt(float dt, const input_t* in);
void sys_anim_controller_adapt(float dt, const input_t* in);

// from ecs_proximity.c
void sys_prox_build_adapt(float dt, const input_t* in);
void sys_billboards_adapt(float dt, const input_t* in);

// =============== Registration =========
static void ecs_register_builtin_systems(void)
{
    ecs_register_system(PHASE_INPUT,    0,   sys_input_adapt, "input");

    ecs_register_system(PHASE_SIM_PRE,  100, sys_prox_build_adapt,      "proximity_view");
    ecs_register_system(PHASE_SIM_PRE,  200, sys_anim_controller_adapt, "animation_controller");

    ecs_register_system(PHASE_PHYSICS,  100, sys_physics_adapt, "physics");
    ecs_register_system(PHASE_PHYSICS,  200, sys_bounds_adapt,  "bounds");

    ecs_register_system(PHASE_SIM_POST, 200, sys_billboards_adapt, "billboards");

    ecs_register_system(PHASE_PRESENT,  100, sys_anim_sprite_adapt,"sprite_anim");

    ecs_register_system(PHASE_DEBUG,    100, sys_debug_binds_adapt, "debug_binds");
}
