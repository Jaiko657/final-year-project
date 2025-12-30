#include "modules/ecs/ecs_internal.h"
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
cmp_liftable_t  cmp_liftable[ECS_MAX_ENTITIES];
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

void liftable_stub_set_player(ecs_entity_t e)
{
    g_player = e;
}

float clampf(float v, float a, float b)
{
    return (v < a) ? a : ((v > b) ? b : v);
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
