#include "modules/ecs/ecs_internal.h"
#include "modules/renderer/renderer.h"
#include "modules/world/world.h"

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

static ecs_entity_t g_player = {0, 0};

bool ecs_alive_idx(int i)
{
    return ecs_gen[i] != 0;
}

int ent_index_checked(ecs_entity_t e)
{
    return (e.idx < ECS_MAX_ENTITIES && ecs_gen[e.idx] == e.gen && e.gen != 0) ? (int)e.idx : -1;
}

ecs_entity_t find_player_handle(void)
{
    return g_player;
}

void grav_gun_stub_set_player(ecs_entity_t e)
{
    g_player = e;
}

float clampf(float v, float a, float b)
{
    return (v < a) ? a : ((v > b) ? b : v);
}

ecs_entity_t handle_from_index(int i)
{
    return (ecs_entity_t){ (uint32_t)i, ecs_gen[i] };
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

bool renderer_screen_to_world(float screen_x, float screen_y, float* out_x, float* out_y)
{
    if (out_x) *out_x = screen_x;
    if (out_y) *out_y = screen_y;
    return true;
}

int world_subtile_size(void)
{
    return 16;
}

void world_size_px(int* out_w, int* out_h)
{
    if (out_w) *out_w = 1000;
    if (out_h) *out_h = 1000;
}

bool world_is_walkable_subtile(int sx, int sy)
{
    (void)sx; (void)sy;
    return true;
}
