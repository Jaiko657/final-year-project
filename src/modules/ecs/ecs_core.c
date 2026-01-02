#include "modules/ecs/ecs_internal.h"
#include "modules/core/logger.h"
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
cmp_grav_gun_t  cmp_grav_gun[ECS_MAX_ENTITIES];
cmp_door_t      cmp_door[ECS_MAX_ENTITIES];

// ========== O(1) create/delete ==========
static int free_stack[ECS_MAX_ENTITIES];
static int free_top = 0;
static uint8_t ecs_destroy_state[ECS_MAX_ENTITIES];

enum {
    ECS_DESTROY_NONE = 0,
    ECS_DESTROY_MARKED = 1,
    ECS_DESTROY_CLEANED = 2
};

static ecs_component_hook_fn cmp_on_destroy_table[ENUM_COMPONENT_COUNT];
static ecs_component_hook_fn phys_body_create_hook = NULL;

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

static void cmp_on_destroy_noop(int idx)
{
    (void)idx;
}

void ecs_register_component_destroy_hook(ComponentEnum comp, ecs_component_hook_fn fn)
{
    if (comp < 0 || comp >= ENUM_COMPONENT_COUNT) return;
    cmp_on_destroy_table[comp] = fn ? fn : cmp_on_destroy_noop;
}

void ecs_register_phys_body_create_hook(ecs_component_hook_fn fn)
{
    phys_body_create_hook = fn;
}

static void ecs_init_destroy_table(void)
{
    for (int i = 0; i < ENUM_COMPONENT_COUNT; ++i) {
        cmp_on_destroy_table[i] = cmp_on_destroy_noop;
    }
    phys_body_create_hook = NULL;
}

static void try_create_phys_body(int i){
    const uint32_t req = (CMP_POS | CMP_COL | CMP_PHYS_BODY);
    if ((ecs_mask[i] & req) != req) return;
    if (cmp_phys_body[i].created) return;
    if (phys_body_create_hook) {
        phys_body_create_hook(i);
    }
}

// forward
// =============== Public: lifecycle ========
void ecs_init(void){
    memset(ecs_mask, 0, sizeof(ecs_mask));
    memset(ecs_gen,  0, sizeof(ecs_gen));
    memset(ecs_next_gen, 0, sizeof(ecs_next_gen));
    memset(ecs_destroy_state, 0, sizeof(ecs_destroy_state));
    free_top = 0;
    ecs_init_destroy_table();
    ecs_anim_reset_allocator();
    for (int i = ECS_MAX_ENTITIES - 1; i >= 0; --i) {
        free_stack[free_top++] = i;
    }

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

static void ecs_cleanup_entity(int idx)
{
    uint32_t mask = ecs_mask[idx];
    for (int comp = 0; comp < ENUM_COMPONENT_COUNT; ++comp) {
        if (mask & (1u << comp)) {
            cmp_on_destroy_table[comp](idx);
        }
    }
}

static void ecs_finalize_destroy(int idx)
{
    uint32_t g = ecs_gen[idx];
    g = (g + 1) ? (g + 1) : 1;
    ecs_gen[idx] = 0;
    ecs_next_gen[idx] = g;
    ecs_mask[idx] = 0;
    ecs_destroy_state[idx] = ECS_DESTROY_NONE;
    free_stack[free_top++] = idx;
}

// =============== Public: entity ===========
ecs_entity_t ecs_create(void)
{
    if (free_top == 0) {
        LOGC(LOGCAT_ECS, LOG_LVL_ERROR, "ecs: out of entities (max=%d)", ECS_MAX_ENTITIES);
        return ecs_null();
    }
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

    ecs_cleanup_entity(idx);
    ecs_finalize_destroy(idx);
}

void ecs_mark_destroy(ecs_entity_t e)
{
    int idx = ent_index_checked(e);
    if (idx < 0) return;
    if (ecs_destroy_state[idx] == ECS_DESTROY_NONE) {
        ecs_destroy_state[idx] = ECS_DESTROY_MARKED;
    }
}

void ecs_cleanup_marked(void)
{
    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (ecs_destroy_state[i] != ECS_DESTROY_MARKED) continue;
        if (!ecs_alive_idx(i)) {
            ecs_destroy_state[i] = ECS_DESTROY_NONE;
            continue;
        }
        ecs_cleanup_entity(i);
        ecs_destroy_state[i] = ECS_DESTROY_CLEANED;
    }
}

void ecs_destroy_marked(void)
{
    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (ecs_destroy_state[i] == ECS_DESTROY_NONE) continue;
        if (!ecs_alive_idx(i)) {
            ecs_destroy_state[i] = ECS_DESTROY_NONE;
            continue;
        }
        if (ecs_destroy_state[i] == ECS_DESTROY_MARKED) {
            ecs_cleanup_entity(i);
        }
        ecs_finalize_destroy(i);
    }
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

void cmp_add_player(ecs_entity_t e)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    ecs_mask[i] |= CMP_PLAYER;
    if (ecs_mask[i] & CMP_PHYS_BODY) {
        cmp_phys_body[i].category_bits |= PHYS_CAT_PLAYER;
    }
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

void cmp_add_grav_gun(ecs_entity_t e)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    cmp_grav_gun[i] = (cmp_grav_gun_t){
        .state              = GRAV_GUN_STATE_FREE,
        .holder             = ecs_null(),
        .pickup_distance    = 0.0f,
        .pickup_radius      = 0.0f,
        .max_hold_distance  = 0.0f,
        .breakoff_distance  = 0.0f,
        .follow_gain        = 0.0f,
        .max_speed          = 0.0f,
        .damping            = 0.0f,
        .hold_vel_x         = 0.0f,
        .hold_vel_y         = 0.0f,
        .grab_offset_x      = 0.0f,
        .grab_offset_y      = 0.0f,
        .saved_mask_bits    = 0u,
        .saved_mask_valid   = false
    };
    ecs_mask[i] |= CMP_GRAV_GUN;
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
    if (ecs_mask[i] & CMP_PLAYER) {
        cmp_phys_body[i].category_bits |= PHYS_CAT_PLAYER;
    }
    ecs_mask[i] |= CMP_PHYS_BODY;
    try_create_phys_body(i);
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
