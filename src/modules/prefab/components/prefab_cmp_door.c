#include "modules/prefab/prefab_cmp.h"

#include "modules/core/logger.h"

#include <stdio.h>
#include <string.h>

static int parse_door_tiles(const char* s, prefab_door_tile_xy_list_t* out_xy)
{
    if (!s || !out_xy) return 0;
    DA_CLEAR(out_xy);
    const char* p = s;
    while (*p) {
        int tx = 0, ty = 0;
        if (sscanf(p, "%d,%d", &tx, &ty) == 2) {
            DA_APPEND(out_xy, ((door_tile_xy_t){ tx, ty }));
        }
        const char* sep = strchr(p, ';');
        if (!sep) break;
        p = sep + 1;
    }
    return (int)out_xy->size;
}

bool prefab_cmp_door_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_door_t* out_door)
{
    if (!out_door) return false;

    float prox_r = 0.0f;
    prefab_parse_float(prefab_combined_value(comp, obj, "proximity_radius"), &prox_r);

    *out_door = (prefab_cmp_door_t){0};
    out_door->prox_radius = prox_r;

    int tile_count = parse_door_tiles(prefab_combined_value(comp, obj, "door_tiles"), &out_door->tiles);
    if (tile_count == 0 && obj && obj->door_tile_count > 0) {
        DA_RESERVE(&out_door->tiles, (size_t)obj->door_tile_count);
        for (int t = 0; t < obj->door_tile_count; ++t) {
            DA_APPEND(&out_door->tiles, ((door_tile_xy_t){ obj->door_tiles[t][0], obj->door_tiles[t][1] }));
        }
        tile_count = (int)out_door->tiles.size;
    }

    if (tile_count == 0) {
        const char* obj_name = (obj && obj->name) ? obj->name : "(unnamed)";
        const char* layer_name = (obj && obj->layer_name) ? obj->layer_name : "(unknown layer)";
        int obj_id = obj ? obj->id : -1;
        LOGC(LOGCAT_PREFAB, LOG_LVL_ERROR, "prefab door missing tiles (obj=%s layer=%s id=%d): set DOOR.door_tiles or obj door tiles", obj_name, layer_name, obj_id);
    }

    (void)tile_count;
    return true;
}

void prefab_cmp_door_free(prefab_cmp_door_t* door)
{
    if (!door) return;
    DA_FREE(&door->tiles);
    *door = (prefab_cmp_door_t){0};
}
