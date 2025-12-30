#include "modules/tiled/tiled_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void add_object_property(tiled_object_t* obj, char* name, char* value, char* type) {
    if (!obj || !name) {
        free(name);
        free(value);
        free(type);
        return;
    }
    size_t idx = obj->property_count;
    tiled_property_t* tmp = (tiled_property_t*)realloc(obj->properties, (idx + 1) * sizeof(tiled_property_t));
    if (!tmp) {
        free(name);
        free(value);
        free(type);
        return;
    }
    obj->properties = tmp;
    obj->properties[idx].name = name;
    obj->properties[idx].type = type;
    obj->properties[idx].value = value ? value : tiled_xstrdup("");
    obj->property_count = idx + 1;
}

void tiled_free_objects(world_map_t *map) {
    if (!map || !map->objects) return;
    for (size_t i = 0; i < map->object_count; ++i) {
        tiled_object_t *o = &map->objects[i];
        free(o->name);
        free(o->layer_name);
        free(o->animationtype);
        if (o->properties) {
            for (size_t p = 0; p < o->property_count; ++p) {
                free(o->properties[p].name);
                free(o->properties[p].type);
                free(o->properties[p].value);
            }
        }
        free(o->properties);
    }
    free(map->objects);
    map->objects = NULL;
    map->object_count = 0;
}

static void parse_object(struct xml_node *obj_node, const char* layer_name, int layer_z, tiled_object_t *out) {
    memset(out, 0, sizeof(*out));
    tiled_node_attr_int(obj_node, "id", &out->id);
    char* gid_str = tiled_node_attr_strdup(obj_node, "gid");
    if (gid_str) {
        out->gid = (uint32_t)strtoul(gid_str, NULL, 10);
        free(gid_str);
    }
    out->name = tiled_node_attr_strdup(obj_node, "name");
    out->layer_name = layer_name ? tiled_xstrdup(layer_name) : NULL;
    out->layer_z = layer_z;
    char *x = tiled_node_attr_strdup(obj_node, "x");
    char *y = tiled_node_attr_strdup(obj_node, "y");
    char *w = tiled_node_attr_strdup(obj_node, "width");
    char *h = tiled_node_attr_strdup(obj_node, "height");
    if (x) { out->x = (float)atof(x); free(x); }
    if (y) { out->y = (float)atof(y); free(y); }
    if (w) { out->w = (float)atof(w); free(w); }
    if (h) { out->h = (float)atof(h); free(h); }

    size_t child_count = xml_node_children(obj_node);
    for (size_t i = 0; i < child_count; ++i) {
        struct xml_node *child = xml_node_child(obj_node, i);
        if (!tiled_node_name_is(child, "properties")) continue;
        size_t prop_count = xml_node_children(child);
        for (size_t p = 0; p < prop_count; ++p) {
            struct xml_node *prop = xml_node_child(child, p);
            if (!tiled_node_name_is(prop, "property")) continue;
            char *pname = tiled_node_attr_strdup(prop, "name");
            if (!pname) continue;
            char *pval = tiled_node_attr_strdup(prop, "value");
            if (!pval) {
                struct xml_string *pv = xml_node_content(prop);
                pval = tiled_xml_string_dup(pv);
            }
            char *ptype = tiled_node_attr_strdup(prop, "type");
            if (!pval) pval = tiled_xstrdup("");
            if (strcmp(pname, "animationtype") == 0) {
                out->animationtype = tiled_xstrdup(pval);
            } else if (strcmp(pname, "proximity_radius") == 0 && pval) {
                out->proximity_radius = atoi(pval);
            } else if (strcmp(pname, "door_tiles") == 0 && pval) {
                // parsing the format: "x1,y1;x2,y2"
                const char *s = pval;
                int count = 0;
                while (*s && count < 4) {
                    int tx = 0, ty = 0;
                    if (sscanf(s, "%d,%d", &tx, &ty) == 2) {
                        out->door_tiles[count][0] = tx;
                        out->door_tiles[count][1] = ty;
                        count++;
                    }
                    const char *sep = strchr(s, ';');
                    if (!sep) break;
                    s = sep + 1;
                }
                out->door_tile_count = count;
            }
            add_object_property(out, pname, pval, ptype);
        }
    }
}

bool tiled_parse_objects_from_root(struct xml_node *root, world_map_t *out_map) {
    size_t children = xml_node_children(root);
    bool ok = true;

    out_map->objects = NULL;
    out_map->object_count = 0;
    size_t obj_cap = 0;
    for (size_t i = 0; i < children; ++i) {
        struct xml_node *child = xml_node_child(root, i);
        if (!tiled_node_name_is(child, "objectgroup")) continue;
        size_t obj_children = xml_node_children(child);
        for (size_t j = 0; j < obj_children; ++j) {
            struct xml_node *obj = xml_node_child(child, j);
            if (!tiled_node_name_is(obj, "object")) continue;
            if (out_map->object_count == obj_cap) {
                obj_cap = obj_cap ? obj_cap * 2 : 8;
                tiled_object_t *tmp = (tiled_object_t *)realloc(out_map->objects, obj_cap * sizeof(tiled_object_t));
                if (!tmp) { ok = false; break; }
                out_map->objects = tmp;
            }
            char *layer_name = tiled_node_attr_strdup(child, "name");
            parse_object(obj, layer_name, (int)i, &out_map->objects[out_map->object_count]);
            out_map->object_count++;
            free(layer_name);
        }
        if (!ok) break;
    }
    return ok;
}

const tiled_property_t* tiled_object_get_property(const tiled_object_t* obj, const char* name) {
    if (!obj || !name) return NULL;
    for (size_t i = 0; i < obj->property_count; ++i) {
        const tiled_property_t* p = &obj->properties[i];
        if (p->name && tiled_str_ieq(p->name, name)) {
            return p;
        }
    }
    return NULL;
}

const char* tiled_object_get_property_value(const tiled_object_t* obj, const char* name) {
    const tiled_property_t* p = tiled_object_get_property(obj, name);
    return p ? p->value : NULL;
}
