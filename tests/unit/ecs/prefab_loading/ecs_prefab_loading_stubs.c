#include "ecs_prefab_loading_stubs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs_doors.h"
#include "modules/ecs/ecs_render.h"
#include "modules/core/logger.h"
#include "modules/prefab/prefab_cmp.h"
#include "modules/tiled/tiled.h"

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

int g_cmp_add_position_calls = 0;
int g_cmp_add_size_calls = 0;
float g_cmp_add_size_last_hx = 0.0f;
float g_cmp_add_size_last_hy = 0.0f;
int g_cmp_add_follow_calls = 0;
ecs_entity_t g_cmp_add_follow_last_target = {0, 0};
float g_cmp_add_follow_last_distance = 0.0f;
float g_cmp_add_follow_last_speed = 0.0f;
int g_prefab_load_calls = 0;
char g_prefab_load_path[256];
bool g_prefab_load_result = true;
int g_log_warn_calls = 0;

bool g_prefab_cmp_follow_result = false;
prefab_cmp_follow_t g_prefab_cmp_follow_out = {0};

void ecs_prefab_loading_stub_reset(void)
{
    memset(ecs_mask, 0, sizeof(ecs_mask));
    memset(ecs_gen, 0, sizeof(ecs_gen));
    g_player = ecs_null();
    g_cmp_add_position_calls = 0;
    g_cmp_add_size_calls = 0;
    g_cmp_add_size_last_hx = 0.0f;
    g_cmp_add_size_last_hy = 0.0f;
    g_cmp_add_follow_calls = 0;
    g_cmp_add_follow_last_target = (ecs_entity_t){0, 0};
    g_cmp_add_follow_last_distance = 0.0f;
    g_cmp_add_follow_last_speed = 0.0f;
    g_prefab_load_calls = 0;
    g_prefab_load_path[0] = '\0';
    g_prefab_load_result = true;
    g_log_warn_calls = 0;
    g_prefab_cmp_follow_result = false;
    g_prefab_cmp_follow_out = (prefab_cmp_follow_t){0};
}

void ecs_prefab_loading_stub_set_player(ecs_entity_t e)
{
    g_player = e;
}

ecs_entity_t ecs_create(void)
{
    ecs_gen[0] = 1;
    return (ecs_entity_t){0, 1};
}

int ent_index_checked(ecs_entity_t e)
{
    return (e.idx < ECS_MAX_ENTITIES && ecs_gen[e.idx] == e.gen && e.gen != 0) ? (int)e.idx : -1;
}

ecs_entity_t ecs_find_player(void)
{
    return g_player;
}

void cmp_add_position(ecs_entity_t e, float x, float y)
{
    int idx = ent_index_checked(e);
    if (idx < 0) return;
    g_cmp_add_position_calls++;
    cmp_pos[idx] = (cmp_position_t){ x, y };
    ecs_mask[idx] |= CMP_POS;
}

void cmp_add_velocity(ecs_entity_t e, float x, float y, facing_t direction)
{
    (void)e; (void)x; (void)y; (void)direction;
}

void cmp_add_phys_body(ecs_entity_t e, PhysicsType type, float mass)
{
    (void)e; (void)type; (void)mass;
}

void cmp_add_size(ecs_entity_t e, float hx, float hy)
{
    (void)e;
    g_cmp_add_size_calls++;
    g_cmp_add_size_last_hx = hx;
    g_cmp_add_size_last_hy = hy;
}

void cmp_add_sprite_path(ecs_entity_t e, const char* path, rectf src, float ox, float oy)
{
    (void)e; (void)path; (void)src; (void)ox; (void)oy;
}

void cmp_add_anim(ecs_entity_t e, int frame_w, int frame_h, int anim_count, const int* frames_per_anim, const anim_frame_coord_t* frames, int frame_buffer_width, float fps)
{
    (void)e; (void)frame_w; (void)frame_h; (void)anim_count;
    (void)frames_per_anim; (void)frames; (void)fps; (void)frame_buffer_width;
}

void cmp_add_player(ecs_entity_t e)
{
    (void)e;
}

void cmp_add_inventory(ecs_entity_t e)
{
    (void)e;
}

void cmp_add_item(ecs_entity_t e, item_kind_t k)
{
    (void)e; (void)k;
}

void cmp_add_vendor(ecs_entity_t e, item_kind_t sells, int price)
{
    (void)e; (void)sells; (void)price;
}

void cmp_add_follow(ecs_entity_t e, ecs_entity_t target, float desired_distance, float max_speed)
{
    int idx = ent_index_checked(e);
    if (idx < 0) return;
    g_cmp_add_follow_calls++;
    g_cmp_add_follow_last_target = target;
    g_cmp_add_follow_last_distance = desired_distance;
    g_cmp_add_follow_last_speed = max_speed;
    cmp_follow[idx].target = target;
    cmp_follow[idx].desired_distance = desired_distance;
    cmp_follow[idx].max_speed = max_speed;
    ecs_mask[idx] |= CMP_FOLLOW;
}

void cmp_add_grav_gun(ecs_entity_t e)
{
    int idx = ent_index_checked(e);
    if (idx < 0) return;
    ecs_mask[idx] |= CMP_GRAV_GUN;
}

void cmp_add_trigger(ecs_entity_t e, float pad, uint32_t target_mask)
{
    (void)e; (void)pad; (void)target_mask;
}

void cmp_add_billboard(ecs_entity_t e, const char* text, float y_off, float linger, billboard_state_t state)
{
    (void)e; (void)text; (void)y_off; (void)linger; (void)state;
}

void cmp_add_door(ecs_entity_t e, float prox_radius, int tile_count, const door_tile_xy_t* tile_xy)
{
    (void)e; (void)prox_radius; (void)tile_count; (void)tile_xy;
}

const char* tiled_object_get_property_value(const tiled_object_t* obj, const char* name)
{
    if (!obj || !name) return NULL;
    for (size_t i = 0; i < obj->property_count; ++i) {
        if (obj->properties[i].name && strcmp(obj->properties[i].name, name) == 0) {
            return obj->properties[i].value;
        }
    }
    return NULL;
}

bool prefab_load(const char* path, prefab_t* out_prefab)
{
    g_prefab_load_calls++;
    snprintf(g_prefab_load_path, sizeof(g_prefab_load_path), "%s", path ? path : "");
    if (!g_prefab_load_result) return false;
    if (out_prefab) {
        memset(out_prefab, 0, sizeof(*out_prefab));
    }
    return true;
}

void prefab_free(prefab_t* prefab)
{
    (void)prefab;
}

bool prefab_cmp_pos_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_pos_t* out_pos)
{
    (void)comp; (void)obj; (void)out_pos;
    return false;
}

bool prefab_cmp_vel_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_vel_t* out_vel)
{
    (void)comp; (void)obj; (void)out_vel;
    return false;
}

bool prefab_cmp_phys_body_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_phys_body_t* out_body)
{
    (void)comp; (void)obj; (void)out_body;
    return false;
}

bool prefab_cmp_spr_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_spr_t* out_spr)
{
    (void)comp; (void)obj; (void)out_spr;
    return false;
}

bool prefab_cmp_anim_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_anim_t* out_anim)
{
    (void)comp; (void)obj; (void)out_anim;
    return false;
}

void prefab_cmp_anim_free(prefab_cmp_anim_t* anim)
{
    (void)anim;
}

bool prefab_cmp_item_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_item_t* out_item)
{
    (void)comp; (void)obj; (void)out_item;
    return false;
}

bool prefab_cmp_vendor_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_vendor_t* out_vendor)
{
    (void)comp; (void)obj; (void)out_vendor;
    return false;
}

bool prefab_cmp_follow_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_follow_t* out_follow)
{
    (void)comp; (void)obj;
    if (g_prefab_cmp_follow_result && out_follow) {
        *out_follow = g_prefab_cmp_follow_out;
    }
    return g_prefab_cmp_follow_result;
}

bool prefab_cmp_col_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_col_t* out_col)
{
    (void)comp; (void)obj; (void)out_col;
    return false;
}

bool prefab_cmp_grav_gun_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_grav_gun_t* out_grav_gun)
{
    (void)comp; (void)obj; (void)out_grav_gun;
    return false;
}

bool prefab_cmp_trigger_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_trigger_t* out_trigger)
{
    (void)comp; (void)obj; (void)out_trigger;
    return false;
}

bool prefab_cmp_billboard_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_billboard_t* out_billboard)
{
    (void)comp; (void)obj; (void)out_billboard;
    return false;
}

bool prefab_cmp_door_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_door_t* out_door)
{
    (void)comp; (void)obj; (void)out_door;
    return false;
}

void prefab_cmp_door_free(prefab_cmp_door_t* out_door)
{
    (void)out_door;
}

v2f prefab_object_position_default(const tiled_object_t* obj)
{
    if (!obj) return v2f_make(0.0f, 0.0f);
    return v2f_make(obj->x, obj->y);
}

bool prefab_parse_float(const char* s, float* out)
{
    if (!s || !out) return false;
    *out = (float)atof(s);
    return true;
}

bool log_would_log(log_level_t lvl)
{
    (void)lvl;
    return true;
}

void log_msg(log_level_t lvl, const log_cat_t* cat, const char* fmt, ...)
{
    (void)cat; (void)fmt;
    if (lvl == LOG_LVL_WARN) g_log_warn_calls++;
}
