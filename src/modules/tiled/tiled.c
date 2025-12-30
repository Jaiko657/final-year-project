#include "modules/tiled/tiled.h"
#include "modules/tiled/tiled_internal.h"
#include "modules/core/logger.h"

#include <stdlib.h>

bool tiled_load_map(const char *tmx_path, world_map_t *out_map) {
    if (!out_map) return false;
    *out_map = (world_map_t){0};
    tiled_reset_tile_anim_arena();

    struct xml_document *doc = tiled_load_xml_document(tmx_path);
    if (!doc) {
        LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Failed to parse TMX: %s", tmx_path);
        return false;
    }
    struct xml_node *root = xml_document_root(doc);
    if (!root || !tiled_node_name_is(root, "map")) {
        LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Unexpected TMX root");
        xml_document_free(doc, true);
        return false;
    }

    if (!tiled_node_attr_int(root, "width", &out_map->width) ||
        !tiled_node_attr_int(root, "height", &out_map->height) ||
        !tiled_node_attr_int(root, "tilewidth", &out_map->tilewidth) ||
        !tiled_node_attr_int(root, "tileheight", &out_map->tileheight)) {
        LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Map missing required attributes");
        xml_document_free(doc, true);
        return false;
    }

    bool ok = tiled_parse_tilesets_from_root(root, tmx_path, out_map);
    if (!ok) {
        xml_document_free(doc, true);
        tiled_free_map(out_map);
        return false;
    }

    ok = tiled_parse_objects_from_root(root, out_map) && ok;
    ok = tiled_parse_layers_from_root(root, out_map) && ok;

    xml_document_free(doc, true);

    if (!ok) {
        tiled_free_map(out_map);
        return false;
    }
    return true;
}

void tiled_free_map(world_map_t *map) {
    if (!map) return;
    for (size_t i = 0; i < map->layer_count; ++i) {
        tiled_layer_t *l = &map->layers[i];
        free(l->name);
        free(l->gids);
    }
    free(map->layers);
    if (map->tilesets) {
        for (size_t i = 0; i < map->tileset_count; ++i) {
            tiled_tileset_t *ts = &map->tilesets[i];
            free(ts->colliders);
            free(ts->no_merge_collider);
            tiled_free_tileset_anims(ts);
            free(ts->render_painters);
            free(ts->painter_offset);
            free(ts->image_path);
        }
    }
    free(map->tilesets);
    tiled_free_objects(map);
    *map = (world_map_t){0};
    tiled_reset_tile_anim_arena();
}
