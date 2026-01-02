#include "modules/ecs/ecs_internal.h"

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

bool ecs_alive_idx(int i)
{
    return ecs_gen[i] != 0;
}

bool ecs_alive_handle(ecs_entity_t e)
{
    return (e.idx < ECS_MAX_ENTITIES && ecs_gen[e.idx] == e.gen && e.gen != 0);
}

int ent_index_checked(ecs_entity_t e)
{
    return ecs_alive_handle(e) ? (int)e.idx : -1;
}

ecs_entity_t handle_from_index(int i)
{
    return (ecs_entity_t){ (uint32_t)i, ecs_gen[i] };
}

float clampf(float v, float a, float b)
{
    return (v < a) ? a : ((v > b) ? b : v);
}
