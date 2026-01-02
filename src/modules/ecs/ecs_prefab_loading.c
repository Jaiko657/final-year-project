#include "modules/ecs/ecs_prefab_loading.h"

#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs_render.h"
#include "modules/ecs/ecs_doors.h"
#include "modules/ecs/ecs_game.h"
#include "modules/core/logger.h"
#include "modules/prefab/prefab_cmp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* xstrdup_local(const char* s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static const char* object_prop_only(const tiled_object_t* obj, const char* comp_name, const char* field)
{
    if (!obj || !field || !comp_name) return NULL;
    char key[128];
    snprintf(key, sizeof(key), "%s.%s", comp_name, field);
    const char* v = tiled_object_get_property_value(obj, key);
    if (v) return v;
    return tiled_object_get_property_value(obj, field);
}

typedef struct prefab_built_entity_t {
    uint32_t present_mask;
    uint32_t override_mask;

    bool has_pos;
    prefab_cmp_pos_t pos;

    bool has_vel;
    prefab_cmp_vel_t vel;

    bool has_phys_body;
    prefab_cmp_phys_body_t phys_body;

    bool has_spr;
    prefab_cmp_spr_t spr;

    bool has_anim;
    prefab_cmp_anim_t anim;

    bool has_plastic;
    bool has_storage;

    bool has_follow;
    prefab_cmp_follow_t follow;

    bool has_col;
    prefab_cmp_col_t col;

    bool has_grav_gun;
    prefab_cmp_grav_gun_t grav_gun;

    bool has_trigger;
    prefab_cmp_trigger_t trigger;

    bool has_billboard;
    prefab_cmp_billboard_t billboard;

    bool has_door;
    prefab_cmp_door_t door;

    bool has_player;
} prefab_built_entity_t;

static void prefab_built_entity_free(prefab_built_entity_t* built)
{
    if (!built) return;
    if (built->has_door) prefab_cmp_door_free(&built->door);
    if (built->has_anim) prefab_cmp_anim_free(&built->anim);
    *built = (prefab_built_entity_t){0};
}

static prefab_built_entity_t prefab_build_entity_components(const prefab_t* prefab, const tiled_object_t* obj)
{
    prefab_built_entity_t built = {0};
    if (!prefab) return built;

    for (size_t i = 0; i < prefab->component_count; ++i) {
        const prefab_component_t* comp = &prefab->components[i];
        built.present_mask |= (1u << comp->id);
        if (comp->override_after_spawn) built.override_mask |= (1u << comp->id);

        switch (comp->id) {
            case ENUM_POS:       built.has_pos = prefab_cmp_pos_build(comp, obj, &built.pos); break;
            case ENUM_VEL:       built.has_vel = prefab_cmp_vel_build(comp, obj, &built.vel); break;
            case ENUM_PHYS_BODY: built.has_phys_body = prefab_cmp_phys_body_build(comp, obj, &built.phys_body); break;
            case ENUM_SPR:       built.has_spr = prefab_cmp_spr_build(comp, obj, &built.spr); break;
            case ENUM_ANIM:      built.has_anim = prefab_cmp_anim_build(comp, obj, &built.anim); break;
            case ENUM_PLAYER:    built.has_player = true; break;
            case ENUM_PLASTIC:   built.has_plastic = true; break;
            case ENUM_STORAGE:   built.has_storage = true; break;
            case ENUM_FOLLOW:    built.has_follow = prefab_cmp_follow_build(comp, obj, &built.follow); break;
            case ENUM_COL:       built.has_col = prefab_cmp_col_build(comp, obj, &built.col); break;
            case ENUM_GRAV_GUN:  built.has_grav_gun = prefab_cmp_grav_gun_build(comp, obj, &built.grav_gun); break;
            case ENUM_TRIGGER:   built.has_trigger = prefab_cmp_trigger_build(comp, obj, &built.trigger); break;
            case ENUM_BILLBOARD: built.has_billboard = prefab_cmp_billboard_build(comp, obj, &built.billboard); break;
            case ENUM_DOOR:
                if (built.has_door) prefab_cmp_door_free(&built.door);
                built.has_door = prefab_cmp_door_build(comp, obj, &built.door);
                break;
            default:
                LOGC(LOGCAT_PREFAB, LOG_LVL_WARN, "prefab ecs: component %u not handled", (unsigned)comp->id);
                break;
        }
    }
    return built;
}

static void prefab_apply_overrides(prefab_built_entity_t* built, const tiled_object_t* obj)
{
    if (!built) return;

    const bool prefab_has_pos = (built->present_mask & CMP_POS) != 0;
    const bool override_pos = (built->override_mask & CMP_POS) != 0;
    if (!prefab_has_pos || override_pos) {
        float x = prefab_object_position_default(obj).x;
        float y = prefab_object_position_default(obj).y;
        prefab_parse_float(object_prop_only(obj, "POS", "x"), &x);
        prefab_parse_float(object_prop_only(obj, "POS", "y"), &y);
        built->pos = (prefab_cmp_pos_t){ x, y };
        built->has_pos = true;
    }

    const bool prefab_has_col = (built->present_mask & CMP_COL) != 0;
    const bool override_col = (built->override_mask & CMP_COL) != 0;
    if (prefab_has_col && override_col && obj) {
        float hx = 0.0f, hy = 0.0f;
        const bool have_hx = prefab_parse_float(object_prop_only(obj, "COL", "hx"), &hx);
        const bool have_hy = prefab_parse_float(object_prop_only(obj, "COL", "hy"), &hy);
        if (!have_hx && obj->w > 0.0f) hx = obj->w * 0.5f;
        if (!have_hy && obj->h > 0.0f) hy = obj->h * 0.5f;
        built->col = (prefab_cmp_col_t){ hx, hy };
        built->has_col = true;
    }
}

static void prefab_add_to_ecs(ecs_entity_t e, const prefab_built_entity_t* built)
{
    if (!built) return;

    if (built->has_pos) cmp_add_position(e, built->pos.x, built->pos.y);
    if (built->has_vel) cmp_add_velocity(e, built->vel.x, built->vel.y, built->vel.dir);
    if (built->has_phys_body) cmp_add_phys_body(e, built->phys_body.type, built->phys_body.mass);
    if (built->has_col) cmp_add_size(e, built->col.hx, built->col.hy);

    if (built->has_spr) {
        cmp_add_sprite_path(e, built->spr.path, built->spr.src, built->spr.ox, built->spr.oy);
    } else if ((built->present_mask & CMP_SPR) != 0) {
        LOGC(LOGCAT_PREFAB, LOG_LVL_WARN, "prefab spr missing path");
    }

    if (built->has_anim) {
        cmp_add_anim(
            e,
            built->anim.frame_w,
            built->anim.frame_h,
            built->anim.anim_count,
            built->anim.frames_per_anim,
            built->anim.frames,
            built->anim.frame_buffer_width,
            built->anim.fps);
    }

    if (built->has_player) cmp_add_player(e);
    if (built->has_plastic) cmp_add_plastic(e);
    if (built->has_storage) cmp_add_storage(e, 0);

    if (built->has_follow) {
        ecs_entity_t target = ecs_null();
        if (built->follow.target_kind == PREFAB_FOLLOW_TARGET_PLAYER) {
            target = ecs_find_player();
        } else if (built->follow.target_kind == PREFAB_FOLLOW_TARGET_ENTITY_IDX) {
            target = (ecs_entity_t){ built->follow.target_idx, 0 };
        }
        cmp_add_follow(e, target, built->follow.desired_distance, built->follow.max_speed);

        if (built->follow.have_vision_range) {
            int idx = ent_index_checked(e);
            if (idx >= 0 && (ecs_mask[idx] & CMP_FOLLOW)) {
                cmp_follow[idx].vision_range = built->follow.vision_range;
            }
        }
    }

    if (built->has_grav_gun) {
        cmp_add_grav_gun(e);
        int idx = ent_index_checked(e);
        if (idx >= 0 && (ecs_mask[idx] & CMP_GRAV_GUN)) {
            if (built->grav_gun.has_pickup_distance) cmp_grav_gun[idx].pickup_distance = built->grav_gun.pickup_distance;
            if (built->grav_gun.has_pickup_radius) cmp_grav_gun[idx].pickup_radius = built->grav_gun.pickup_radius;
            if (built->grav_gun.has_max_hold_distance) cmp_grav_gun[idx].max_hold_distance = built->grav_gun.max_hold_distance;
            if (built->grav_gun.has_breakoff_distance) cmp_grav_gun[idx].breakoff_distance = built->grav_gun.breakoff_distance;
            if (built->grav_gun.has_follow_gain) cmp_grav_gun[idx].follow_gain = built->grav_gun.follow_gain;
            if (built->grav_gun.has_max_speed) cmp_grav_gun[idx].max_speed = built->grav_gun.max_speed;
            if (built->grav_gun.has_damping) cmp_grav_gun[idx].damping = built->grav_gun.damping;
        }
    }

    if (built->has_trigger) cmp_add_trigger(e, built->trigger.pad, built->trigger.target_mask);
    if (built->has_billboard) cmp_add_billboard(e, built->billboard.text, built->billboard.y_offset, built->billboard.linger, built->billboard.state);

    if (built->has_door) {
        cmp_add_door(e, built->door.prox_radius, (int)built->door.tiles.size, built->door.tiles.size ? built->door.tiles.data : NULL);
    }
}

ecs_entity_t ecs_prefab_spawn_entity(const prefab_t* prefab, const tiled_object_t* obj)
{
    ecs_entity_t e = ecs_create();
    if (!prefab) return e;

    // Phase 1: build all components into plain structs (no ECS).
    prefab_built_entity_t built = prefab_build_entity_components(prefab, obj);

    // Phase 2: apply overrides to the built data (e.g., tiled object properties).
    prefab_apply_overrides(&built, obj);

    // Phase 3: add built components to ECS.
    prefab_add_to_ecs(e, &built);

    prefab_built_entity_free(&built);

    return e;
}

ecs_entity_t ecs_prefab_spawn_entity_from_path(const char* prefab_path, const tiled_object_t* obj)
{
    prefab_t prefab;
    if (!prefab_load(prefab_path, &prefab)) {
        LOGC(LOGCAT_PREFAB, LOG_LVL_ERROR, "ecs_prefab: could not load %s", prefab_path ? prefab_path : "(null)");
        return ecs_null();
    }
    ecs_entity_t e = ecs_prefab_spawn_entity(&prefab, obj);
    prefab_free(&prefab);
    return e;
}

static char* join_relative_path(const char* base_path, const char* rel)
{
    if (!rel || rel[0] == '\0') return NULL;
    if (!base_path || rel[0] == '/' || rel[0] == '\\') return xstrdup_local(rel);
    const char* slash = strrchr(base_path, '/');
#ifdef _WIN32
    const char* bslash = strrchr(base_path, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    if (!slash) return xstrdup_local(rel);
    size_t dir_len = (size_t)(slash - base_path) + 1;
    size_t rel_len = strlen(rel);
    char* buf = (char*)malloc(dir_len + rel_len + 1);
    if (!buf) return NULL;
    memcpy(buf, base_path, dir_len);
    memcpy(buf + dir_len, rel, rel_len);
    buf[dir_len + rel_len] = '\0';
    return buf;
}

size_t ecs_prefab_spawn_from_map(const world_map_t* map, const char* tmx_path)
{
    if (!map) return 0;

    size_t spawned = 0;
    for (size_t i = 0; i < map->object_count; ++i) {
        const tiled_object_t* obj = &map->objects[i];
        const char* prefab_rel = tiled_object_get_property_value(obj, "entityprefab");
        if (!prefab_rel) continue;

        char* resolved = join_relative_path(tmx_path, prefab_rel);
        const char* path = resolved ? resolved : prefab_rel;
        ecs_entity_t e = ecs_prefab_spawn_entity_from_path(path, obj);
        if (resolved) free(resolved);
        int idx = ent_index_checked(e);
        if (idx >= 0) spawned++;
    }

    return spawned;
}
