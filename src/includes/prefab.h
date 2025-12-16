#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "ecs.h"
#include "tiled.h"

typedef struct prefab_t prefab_t;
typedef struct prefab_component_t prefab_component_t;

bool prefab_load(const char* path, prefab_t* out_prefab);
void prefab_free(prefab_t* prefab);

ecs_entity_t prefab_spawn_entity(const prefab_t* prefab, const tiled_object_t* obj);
ecs_entity_t prefab_spawn_from_path(const char* prefab_path, const tiled_object_t* obj);
size_t prefab_spawn_from_map(const tiled_map_t* map, const char* tmx_path);
