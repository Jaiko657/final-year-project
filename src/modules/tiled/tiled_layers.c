#include "modules/tiled/tiled_internal.h"
#include "modules/core/logger.h"

#include <stdlib.h>
#include <string.h>

static bool parse_layer(struct xml_node *layer_node, tiled_layer_t *out_layer) {
    memset(out_layer, 0, sizeof(*out_layer));
    out_layer->name = tiled_node_attr_strdup(layer_node, "name");
    if (!tiled_node_attr_int(layer_node, "width", &out_layer->width) ||
        !tiled_node_attr_int(layer_node, "height", &out_layer->height)) {
        LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Layer missing width/height");
        return false;
    }

    size_t child_count = xml_node_children(layer_node);
    for (size_t i = 0; i < child_count; ++i) {
        struct xml_node *child = xml_node_child(layer_node, i);
        if (!tiled_node_name_is(child, "properties")) continue;
        size_t prop_count = xml_node_children(child);
        for (size_t p = 0; p < prop_count; ++p) {
            struct xml_node *prop = xml_node_child(child, p);
            if (!tiled_node_name_is(prop, "property")) continue;
            char *pname = tiled_node_attr_strdup(prop, "name");
            if (!pname) continue;
            if (strcmp(pname, "collision") == 0) {
                char *pval = tiled_node_attr_strdup(prop, "value");
                out_layer->collision = tiled_parse_bool_str(pval);
                free(pval);
            }
            free(pname);
        }
    }

    struct xml_node *data_node = NULL;
    for (size_t i = 0; i < child_count; ++i) {
        struct xml_node *child = xml_node_child(layer_node, i);
        if (tiled_node_name_is(child, "data")) {
            data_node = child;
            break;
        }
    }
    if (!data_node) {
        LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Layer missing <data>");
        return false;
    }

    size_t total = (size_t)out_layer->width * (size_t)out_layer->height;
    out_layer->gids = (uint32_t *)calloc(total, sizeof(uint32_t));
    if (!out_layer->gids) {
        return false;
    }

    bool has_chunk = false;
    size_t data_children = xml_node_children(data_node);
    for (size_t i = 0; i < data_children; ++i) {
        if (tiled_node_name_is(xml_node_child(data_node, i), "chunk")) {
            has_chunk = true;
            break;
        }
    }

    bool ok = true;
    if (has_chunk) {
        for (size_t i = 0; i < data_children; ++i) {
            struct xml_node *chunk = xml_node_child(data_node, i);
            if (!tiled_node_name_is(chunk, "chunk")) continue;

            int cx = 0, cy = 0, cw = 0, ch = 0;
            if (!tiled_node_attr_int(chunk, "x", &cx) ||
                !tiled_node_attr_int(chunk, "y", &cy) ||
                !tiled_node_attr_int(chunk, "width", &cw) ||
                !tiled_node_attr_int(chunk, "height", &ch)) {
                LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Chunk missing position/size in layer '%s'", out_layer->name ? out_layer->name : "(unnamed)");
                ok = false;
                break;
            }

            struct xml_string *chunk_str = xml_node_content(chunk);
            char *csv = tiled_xml_string_dup(chunk_str);
            if (!csv) {
                LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Failed to copy chunk CSV for layer '%s'", out_layer->name ? out_layer->name : "(unnamed)");
                ok = false;
                break;
            }

            size_t chunk_total = (size_t)cw * (size_t)ch;
            uint32_t *chunk_gids = (uint32_t *)calloc(chunk_total, sizeof(uint32_t));
            if (!chunk_gids) {
                free(csv);
                ok = false;
                break;
            }
            bool parsed_chunk = tiled_parse_csv_gids(csv, chunk_total, chunk_gids);
            free(csv);
            if (!parsed_chunk) {
                free(chunk_gids);
                LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "CSV parse mismatch for chunk in layer '%s'", out_layer->name ? out_layer->name : "(unnamed)");
                ok = false;
                break;
            }

            for (int y = 0; y < ch; ++y) {
                for (int x = 0; x < cw; ++x) {
                    size_t di = ((size_t)(cy + y) * (size_t)out_layer->width) + (size_t)(cx + x);
                    size_t si = (size_t)y * (size_t)cw + (size_t)x;
                    if ((cx + x) < 0 || (cy + y) < 0 || di >= total) continue;
                    out_layer->gids[di] = chunk_gids[si];
                }
            }
            free(chunk_gids);
        }
    } else {
        struct xml_string *data_str = xml_node_content(data_node);
        char *csv = tiled_xml_string_dup(data_str);
        if (!csv) {
            LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Failed to copy layer CSV");
            ok = false;
        }
        if (ok) {
            bool parsed = tiled_parse_csv_gids(csv, total, out_layer->gids);
            free(csv);
            if (!parsed) {
                LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "CSV parse mismatch for layer '%s'", out_layer->name ? out_layer->name : "(unnamed)");
                ok = false;
            }
        } else {
            free(csv);
        }
    }
    if (!ok) {
        free(out_layer->gids);
        free(out_layer->name);
        *out_layer = (tiled_layer_t){0};
    }
    return ok;
}

bool tiled_parse_layers_from_root(struct xml_node *root, world_map_t *out_map) {
    size_t children = xml_node_children(root);

    size_t layer_cap = 4;
    out_map->layers = (tiled_layer_t *)calloc(layer_cap, sizeof(tiled_layer_t));
    if (!out_map->layers) {
        return false;
    }

    bool ok = true;
    for (size_t i = 0; i < children; ++i) {
        struct xml_node *child = xml_node_child(root, i);
        if (!tiled_node_name_is(child, "layer")) continue;
        if (out_map->layer_count == layer_cap) {
            layer_cap *= 2;
            tiled_layer_t *tmp = (tiled_layer_t *)realloc(out_map->layers, layer_cap * sizeof(tiled_layer_t));
            if (!tmp) { ok = false; break; }
            out_map->layers = tmp;
        }
        if (!parse_layer(child, &out_map->layers[out_map->layer_count])) {
            ok = false;
            break;
        }
        out_map->layers[out_map->layer_count].z_order = (int)i;
        out_map->layer_count++;
    }
    return ok;
}
