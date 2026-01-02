#include "modules/core/engine_types.h"
#include "modules/ecs/ecs.h"
#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs_proximity.h"
#include "modules/ecs/ecs_game.h"
#include "modules/systems/systems.h"
#include "modules/systems/systems_registration.h"
#include "modules/core/toast.h"
#include "modules/core/logger.h"
#include "modules/tiled/tiled.h"
#include "modules/ecs/ecs_prefab_loading.h"
#include "modules/world/world.h"
#include "modules/world/world_renderer.h"
#include "modules/ecs/ecs_door_systems.h"
#include <stdio.h>

bool init_entities(const char* tmx_path)
{
    const world_map_t* map = world_get_map();
    if (!map) {
        LOGC(LOGCAT_ECS, LOG_LVL_ERROR, "init_entities: no tiled map loaded");
        return false;
    }

    ecs_prefab_spawn_from_map(map, tmx_path);
    return true;
}

// ===== Game-side component storage =====
typedef struct { int plastic; int capacity; } cmp_storage_t;

static cmp_storage_t   g_storage[ECS_MAX_ENTITIES];

static const int k_storage_default_capacity = 20;

// ===== Component adders =====
void cmp_add_plastic(ecs_entity_t e)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    ecs_mask[i] |= CMP_PLASTIC;
}

void cmp_add_storage(ecs_entity_t e, int capacity)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    if (capacity <= 0) capacity = k_storage_default_capacity;
    g_storage[i] = (cmp_storage_t){ 0, capacity };
    ecs_mask[i] |= CMP_STORAGE;
    if (ecs_mask[i] & CMP_PHYS_BODY) {
        cmp_phys_body[i].category_bits |= PHYS_CAT_TARDAS;
    }
}

// ===== Gameplay helpers =====
bool game_get_tardas_storage(int* out_plastic, int* out_capacity)
{
    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & CMP_STORAGE) == 0) continue;
        if (out_plastic) *out_plastic = g_storage[i].plastic;
        if (out_capacity) *out_capacity = g_storage[i].capacity;
        return true;
    }
    if (out_plastic) *out_plastic = 0;
    if (out_capacity) *out_capacity = 0;
    return false;
}

bool ecs_game_get_storage(ecs_entity_t e, int* out_plastic, int* out_capacity)
{
    int i = ent_index_checked(e);
    if (i < 0) return false;
    if ((ecs_mask[i] & CMP_STORAGE) == 0) return false;
    if (out_plastic) *out_plastic = g_storage[i].plastic;
    if (out_capacity) *out_capacity = g_storage[i].capacity;
    return true;
}

// ===== Systems: storage =====
static void sys_storage_deposit_impl(void)
{
    ecs_prox_iter_t it = ecs_prox_stay_begin();
    ecs_prox_view_t v;
    while (ecs_prox_stay_next(&it, &v)) {
        int ia = ent_index_checked(v.trigger_owner);
        int ib = ent_index_checked(v.matched_entity);
        if (ia < 0 || ib < 0) continue;

        if ((ecs_mask[ia] & CMP_STORAGE) == 0) continue;
        if ((ecs_mask[ib] & (CMP_PLASTIC | CMP_GRAV_GUN)) != (CMP_PLASTIC | CMP_GRAV_GUN)) continue;

        cmp_grav_gun_t* g = &cmp_grav_gun[ib];
        if (g->state == GRAV_GUN_STATE_HELD) continue;
        if (!g->just_dropped) continue;

        cmp_storage_t* storage = &g_storage[ia];
        if (storage->plastic >= storage->capacity) {
            ui_toast(1.0f, "TARDAS full (%d/%d)", storage->plastic, storage->capacity);
            continue;
        }

        storage->plastic += 1;
        ecs_destroy(v.matched_entity);
        ui_toast(1.0f, "Plastic stored (%d/%d)", storage->plastic, storage->capacity);
    }

    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & CMP_GRAV_GUN) == 0) continue;
        cmp_grav_gun[i].just_dropped = false;
    }
}

// ===== System adapters + registration =====
SYSTEMS_ADAPT_VOID(sys_storage_deposit_adapt, sys_storage_deposit_impl)

void ecs_register_game_systems(void)
{
    // maintain original ordering around billboards
    systems_register(PHASE_SIM_POST, 120, sys_storage_deposit_adapt, "storage_deposit");
    ecs_register_door_systems();
}
