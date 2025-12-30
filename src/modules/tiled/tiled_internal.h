#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "modules/tiled/tiled.h"
#include "xml.h"

char *tiled_xstrdup(const char *s);
char *tiled_xml_string_dup(struct xml_string *xs);
char *tiled_node_attr_strdup(struct xml_node *node, const char *name);
bool tiled_node_attr_int(struct xml_node *node, const char *name, int *out);
bool tiled_node_name_is(struct xml_node *node, const char *name);
bool tiled_file_exists(const char *path);
bool tiled_parse_bool_str(const char *s);
bool tiled_str_ieq(const char* a, const char* b);
char *tiled_scan_attr_in_file(const char *path, const char *tag, const char *attr);
struct xml_document *tiled_load_xml_document(const char *path);
char *tiled_join_relative(const char *base_path, const char *rel);
bool tiled_parse_csv_gids(const char *csv, size_t expected, uint32_t *out);

bool tiled_parse_tilesets_from_root(struct xml_node *root, const char *tmx_path, world_map_t *out_map);
void tiled_free_tileset_anims(tiled_tileset_t *ts);
void tiled_reset_tile_anim_arena(void);

bool tiled_parse_layers_from_root(struct xml_node *root, world_map_t *out_map);

bool tiled_parse_objects_from_root(struct xml_node *root, world_map_t *out_map);
void tiled_free_objects(world_map_t *map);
