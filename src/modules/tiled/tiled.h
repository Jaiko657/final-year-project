#pragma once

#include "modules/common/resource_handles.h"
#include "modules/tiled/tiled_types.h"
#include "modules/world/world_map.h"

bool tiled_load_map(const char *tmx_path, world_map_t *out_map);
void tiled_free_map(world_map_t *map);

typedef struct {
    size_t texture_count;
    tex_handle_t *tilesets; // matches map->tilesets ordering
} tiled_renderer_t;

bool tiled_renderer_init(tiled_renderer_t *r, const world_map_t *map);
void tiled_renderer_shutdown(tiled_renderer_t *r);

const tiled_property_t* tiled_object_get_property(const tiled_object_t* obj, const char* name);
const char* tiled_object_get_property_value(const tiled_object_t* obj, const char* name);
