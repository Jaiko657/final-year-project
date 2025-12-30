#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs_physics.h"
#include "modules/world/world.h"

#include <string.h>

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

bool g_world_has_map = true;
int g_world_subtile = 0;
bool g_world_has_los = true;
bool g_world_walkable = true;
int g_world_resolve_axis_calls = 0;
int g_world_resolve_mtv_calls = 0;
int g_phys_create_calls = 0;

void ecs_system_domains_stub_reset(void)
{
    memset(ecs_mask, 0, sizeof(ecs_mask));
    memset(ecs_gen, 0, sizeof(ecs_gen));
    memset(ecs_next_gen, 0, sizeof(ecs_next_gen));
    memset(cmp_pos, 0, sizeof(cmp_pos));
    memset(cmp_vel, 0, sizeof(cmp_vel));
    memset(cmp_follow, 0, sizeof(cmp_follow));
    memset(cmp_col, 0, sizeof(cmp_col));
    memset(cmp_phys_body, 0, sizeof(cmp_phys_body));
    g_world_has_map = true;
    g_world_subtile = 0;
    g_world_has_los = true;
    g_world_walkable = true;
    g_world_resolve_axis_calls = 0;
    g_world_resolve_mtv_calls = 0;
    g_phys_create_calls = 0;
}

bool ecs_alive_idx(int i)
{
    return ecs_gen[i] != 0;
}

int ent_index_checked(ecs_entity_t e)
{
    return (e.idx < ECS_MAX_ENTITIES && ecs_gen[e.idx] == e.gen && e.gen != 0)
        ? (int)e.idx : -1;
}

int ent_index_unchecked(ecs_entity_t e)
{
    return (int)e.idx;
}

bool ecs_alive_handle(ecs_entity_t e)
{
    return ent_index_checked(e) >= 0;
}

ecs_entity_t handle_from_index(int i)
{
    return (ecs_entity_t){ (uint32_t)i, ecs_gen[i] };
}

int world_subtile_size(void)
{
    return g_world_subtile;
}

bool world_has_line_of_sight(
    float ax, float ay,
    float bx, float by,
    float max_dist,
    float clear_hx,
    float clear_hy)
{
    (void)ax;
    (void)ay;
    (void)bx;
    (void)by;
    (void)max_dist;
    (void)clear_hx;
    (void)clear_hy;
    return g_world_has_los;
}

bool world_is_walkable_rect_px(float x, float y, float hx, float hy)
{
    (void)x;
    (void)y;
    (void)hx;
    (void)hy;
    return g_world_walkable;
}

bool world_has_map(void)
{
    return g_world_has_map;
}

bool world_resolve_rect_axis_px(float* cx, float* cy, float hx, float hy, bool resolve_x)
{
    (void)cx;
    (void)cy;
    (void)hx;
    (void)hy;
    (void)resolve_x;
    g_world_resolve_axis_calls++;
    return false;
}

bool world_resolve_rect_mtv_px(float* cx, float* cy, float hx, float hy)
{
    (void)cx;
    (void)cy;
    (void)hx;
    (void)hy;
    g_world_resolve_mtv_calls++;
    return false;
}

void ecs_phys_body_create_for_entity(int idx)
{
    g_phys_create_calls++;
    if (idx < 0 || idx >= ECS_MAX_ENTITIES) return;
    cmp_phys_body[idx].created = true;
}
