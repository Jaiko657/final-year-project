#pragma once

#include <stddef.h>
#include "modules/ecs/ecs.h"
#include "modules/tiled/tiled.h"
#include "modules/prefab/prefab.h"

ecs_entity_t ecs_prefab_spawn_entity(const prefab_t* prefab, const tiled_object_t* obj);
ecs_entity_t ecs_prefab_spawn_entity_from_path(const char* prefab_path, const tiled_object_t* obj);
size_t ecs_prefab_spawn_from_map(const world_map_t* map, const char* tmx_path);

