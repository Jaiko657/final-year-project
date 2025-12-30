#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs_doors.h"
#include "modules/world/world_door.h"

void ecs_door_on_destroy(int idx)
{
    if (idx < 0 || idx >= ECS_MAX_ENTITIES) return;
    if (!(ecs_mask[idx] & CMP_DOOR)) return;
    if (cmp_door[idx].world_handle != WORLD_DOOR_INVALID_HANDLE) {
        world_door_unregister(cmp_door[idx].world_handle);
        cmp_door[idx].world_handle = WORLD_DOOR_INVALID_HANDLE;
    }
}

void ecs_register_door_component_hooks(void)
{
    ecs_register_component_destroy_hook(ENUM_DOOR, ecs_door_on_destroy);
}

void cmp_add_door(ecs_entity_t e, float prox_radius, int tile_count, const door_tile_xy_t* tile_xy)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    cmp_door_t* d = &cmp_door[i];
    if (d->world_handle != WORLD_DOOR_INVALID_HANDLE) {
        world_door_unregister(d->world_handle);
    }
    *d = (cmp_door_t){0};
    d->prox_radius = prox_radius;
    if (tile_xy && tile_count > 0) {
        d->world_handle = world_door_register(tile_xy, (size_t)tile_count);
    } else {
        d->world_handle = WORLD_DOOR_INVALID_HANDLE;
    }
    d->state = DOOR_CLOSED;
    d->anim_time_ms = 0.0f;
    d->intent_open = false;
    ecs_mask[i] |= CMP_DOOR;
}
