#include "modules/tiled/tiled_internal.h"
#include "modules/asset/bump_alloc.h"
#include "modules/core/logger.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

static bump_alloc_t g_tile_anim_arena;
static const size_t TILE_ANIM_ARENA_BYTES = 512 * 1024;

static bool ensure_tile_anim_arena(void) {
    if (g_tile_anim_arena.data) return true;
    return bump_init(&g_tile_anim_arena, TILE_ANIM_ARENA_BYTES);
}

void tiled_reset_tile_anim_arena(void) {
    if (!g_tile_anim_arena.data) {
        if (!bump_init(&g_tile_anim_arena, TILE_ANIM_ARENA_BYTES)) return;
    }
    bump_reset(&g_tile_anim_arena);
}

// Expects "[abcd],[efgh],[ijkl],[mnop]" where each char is 0/1; builds 4x4 bitmask row-major
static uint16_t parse_collider_mask(const char *s, bool *out_ok) {
    if (out_ok) *out_ok = false;
    if (!s) return 0;
    uint16_t mask = 0;
    int row = 0;
    const char *p = s;
    while (*p && row < 4) {
        while (*p && *p != '[') p++;
        if (!*p) break;
        p++; // skip '['
        for (int col = 0; col < 4 && *p && *p != ']'; ++col, ++p) {
            if (*p == '1') {
                int bit = row * 4 + col;
                mask |= (uint16_t)(1u << bit);
            }
        }
        while (*p && *p != ']') p++;
        if (*p == ']') p++;
        if (*p == ',') p++;
        row++;
    }
    if (out_ok) *out_ok = (row > 0);
    return mask;
}

void tiled_free_tileset_anims(tiled_tileset_t *ts) {
    if (!ts || !ts->anims) return;
    free(ts->anims);
    ts->anims = NULL;
}

static bool parse_tileset(const char *tsx_path, tiled_tileset_t *out_tileset) {
    memset(out_tileset, 0, sizeof(*out_tileset));
    struct xml_document *doc = tiled_load_xml_document(tsx_path);
    if (!doc) {
        LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Failed to parse TSX: %s", tsx_path);
        return false;
    }
    struct xml_node *root = xml_document_root(doc);
    if (!root || !tiled_node_name_is(root, "tileset")) {
        LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Unexpected TSX root");
        xml_document_free(doc, true);
        return false;
    }

    if (!tiled_node_attr_int(root, "tilewidth", &out_tileset->tilewidth) ||
        !tiled_node_attr_int(root, "tileheight", &out_tileset->tileheight) ||
        !tiled_node_attr_int(root, "tilecount", &out_tileset->tilecount) ||
        !tiled_node_attr_int(root, "columns", &out_tileset->columns)) {
        LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Missing tileset attributes");
        xml_document_free(doc, true);
        return false;
    }

    if (!ensure_tile_anim_arena()) {
        xml_document_free(doc, true);
        return false;
    }

    out_tileset->colliders = (uint16_t *)calloc((size_t)out_tileset->tilecount, sizeof(uint16_t));
    out_tileset->no_merge_collider = (bool *)calloc((size_t)out_tileset->tilecount, sizeof(bool));
    out_tileset->anims = (tiled_animation_t *)calloc((size_t)out_tileset->tilecount, sizeof(tiled_animation_t));
    out_tileset->render_painters = (bool *)calloc((size_t)out_tileset->tilecount, sizeof(bool));
    out_tileset->painter_offset = (int *)calloc((size_t)out_tileset->tilecount, sizeof(int));
    if (!out_tileset->colliders || !out_tileset->anims || !out_tileset->no_merge_collider || !out_tileset->render_painters || !out_tileset->painter_offset) {
        free(out_tileset->colliders);
        free(out_tileset->no_merge_collider);
        free(out_tileset->anims);
        free(out_tileset->render_painters);
        free(out_tileset->painter_offset);
        xml_document_free(doc, true);
        return false;
    }

    struct xml_node *image = NULL;
    size_t children = xml_node_children(root);
    for (size_t i = 0; i < children; ++i) {
        struct xml_node *child = xml_node_child(root, i);
        if (tiled_node_name_is(child, "image")) {
            image = child;
        } else if (tiled_node_name_is(child, "tile")) {
            int tile_id = 0;
            if (!tiled_node_attr_int(child, "id", &tile_id)) continue;
            if (tile_id < 0 || tile_id >= out_tileset->tilecount) continue;
            size_t tile_children = xml_node_children(child);
            for (size_t j = 0; j < tile_children; ++j) {
                struct xml_node *node_child = xml_node_child(child, j);
                if (tiled_node_name_is(node_child, "properties")) {
                    size_t prop_count = xml_node_children(node_child);
                    for (size_t p = 0; p < prop_count; ++p) {
                        struct xml_node *prop = xml_node_child(node_child, p);
                        if (!tiled_node_name_is(prop, "property")) continue;
                        char *pname = tiled_node_attr_strdup(prop, "name");
                        if (!pname) continue;
                        if (strcmp(pname, "collider") == 0) {
                            char *pval = tiled_node_attr_strdup(prop, "value");
                            bool ok = false;
                            uint16_t mask = parse_collider_mask(pval, &ok);
                            if (ok) out_tileset->colliders[tile_id] = mask;
                            free(pval);
                        } else if (strcmp(pname, "renderstyle") == 0) {
                            char *pval = tiled_node_attr_strdup(prop, "value");
                            if (pval && strcasecmp(pval, "painters") == 0) {
                                out_tileset->render_painters[tile_id] = true;
                            }
                            free(pval);
                        } else if (strcmp(pname, "painteroffset") == 0) {
                            char *pval = tiled_node_attr_strdup(prop, "value");
                            if (pval) {
                                out_tileset->painter_offset[tile_id] = atoi(pval);
                                free(pval);
                            }
                        } else if (strcmp(pname, "animationtype") == 0) {
                            char *pval = tiled_node_attr_strdup(prop, "value");
                            if (pval && strcasecmp(pval, "door") == 0) {
                                out_tileset->no_merge_collider[tile_id] = true;
                            }
                            free(pval);
                        }
                        free(pname);
                    }
                } else if (tiled_node_name_is(node_child, "animation")) {
                    size_t frame_count = 0;
                    size_t anim_children = xml_node_children(node_child);
                    for (size_t k = 0; k < anim_children; ++k) {
                        if (tiled_node_name_is(xml_node_child(node_child, k), "frame")) frame_count++;
                    }
                    if (frame_count > 0) {
                        tiled_anim_frame_t *frames = bump_alloc_type(&g_tile_anim_arena, tiled_anim_frame_t, frame_count);
                        if (!frames) {
                            free(out_tileset->colliders);
                            free(out_tileset->no_merge_collider);
                            tiled_free_tileset_anims(out_tileset);
                            free(out_tileset->render_painters);
                            free(out_tileset->painter_offset);
                            xml_document_free(doc, true);
                            return false;
                        }
                        memset(frames, 0, frame_count * sizeof(tiled_anim_frame_t));
                        int total_ms = 0;
                        size_t fi = 0;
                        for (size_t k = 0; k < anim_children && fi < frame_count; ++k) {
                            struct xml_node *frame = xml_node_child(node_child, k);
                            if (!tiled_node_name_is(frame, "frame")) continue;
                            tiled_node_attr_int(frame, "tileid", &frames[fi].tile_id);
                            tiled_node_attr_int(frame, "duration", &frames[fi].duration_ms);
                            if (frames[fi].duration_ms < 0) frames[fi].duration_ms = 0;
                            total_ms += frames[fi].duration_ms;
                            fi++;
                        }
                        out_tileset->anims[tile_id].frames = frames;
                        out_tileset->anims[tile_id].frame_count = fi;
                        out_tileset->anims[tile_id].total_duration_ms = total_ms;

                        // Propagate door/no-merge flag to all frame tiles so runtime detection works on animated frames.
                        if (out_tileset->no_merge_collider && out_tileset->no_merge_collider[tile_id]) {
                            for (size_t f = 0; f < fi; ++f) {
                                int fid = frames[f].tile_id;
                                if (fid >= 0 && fid < out_tileset->tilecount) {
                                    out_tileset->no_merge_collider[fid] = true;
                                }
                            }
                        }
                        if (out_tileset->render_painters && out_tileset->render_painters[tile_id]) {
                            for (size_t f = 0; f < fi; ++f) {
                                int fid = frames[f].tile_id;
                                if (fid >= 0 && fid < out_tileset->tilecount) {
                                    out_tileset->render_painters[fid] = true;
                                    if (out_tileset->painter_offset) {
                                        out_tileset->painter_offset[fid] = out_tileset->painter_offset[tile_id];
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (!image) {
        LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "No <image> in TSX");
        free(out_tileset->colliders);
        free(out_tileset->no_merge_collider);
        tiled_free_tileset_anims(out_tileset);
        free(out_tileset->render_painters);
        free(out_tileset->painter_offset);
        xml_document_free(doc, true);
        return false;
    }

    out_tileset->image_path = tiled_node_attr_strdup(image, "source");
    tiled_node_attr_int(image, "width", &out_tileset->image_width);
    tiled_node_attr_int(image, "height", &out_tileset->image_height);

    xml_document_free(doc, true);

    if (!out_tileset->image_path) {
        out_tileset->image_path = tiled_scan_attr_in_file(tsx_path, "<image", "source");
    }
    if (!out_tileset->image_path) {
        LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Tileset missing image source");
        free(out_tileset->colliders);
        free(out_tileset->no_merge_collider);
        tiled_free_tileset_anims(out_tileset);
        free(out_tileset->render_painters);
        free(out_tileset->painter_offset);
        return false;
    }
    return true;
}

static bool parse_tileset_inline(struct xml_node *tileset_node, const char *tmx_path, tiled_tileset_t *out_tileset) {
    memset(out_tileset, 0, sizeof(*out_tileset));
    if (!tiled_node_attr_int(tileset_node, "tilewidth", &out_tileset->tilewidth) ||
        !tiled_node_attr_int(tileset_node, "tileheight", &out_tileset->tileheight) ||
        !tiled_node_attr_int(tileset_node, "tilecount", &out_tileset->tilecount) ||
        !tiled_node_attr_int(tileset_node, "columns", &out_tileset->columns)) {
        LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Inline tileset missing attributes");
        return false;
    }

    if (!ensure_tile_anim_arena()) {
        return false;
    }

    out_tileset->colliders = (uint16_t *)calloc((size_t)out_tileset->tilecount, sizeof(uint16_t));
    out_tileset->no_merge_collider = (bool *)calloc((size_t)out_tileset->tilecount, sizeof(bool));
    out_tileset->anims = (tiled_animation_t *)calloc((size_t)out_tileset->tilecount, sizeof(tiled_animation_t));
    out_tileset->render_painters = (bool *)calloc((size_t)out_tileset->tilecount, sizeof(bool));
    out_tileset->painter_offset = (int *)calloc((size_t)out_tileset->tilecount, sizeof(int));
    if (!out_tileset->colliders || !out_tileset->anims || !out_tileset->no_merge_collider || !out_tileset->render_painters || !out_tileset->painter_offset) {
        free(out_tileset->colliders);
        free(out_tileset->no_merge_collider);
        free(out_tileset->anims);
        free(out_tileset->render_painters);
        free(out_tileset->painter_offset);
        return false;
    }

    struct xml_node *image = NULL;
    size_t children = xml_node_children(tileset_node);
    for (size_t i = 0; i < children; ++i) {
        struct xml_node *child = xml_node_child(tileset_node, i);
        if (tiled_node_name_is(child, "image")) {
            image = child;
        } else if (tiled_node_name_is(child, "tile")) {
            int tile_id = 0;
            if (!tiled_node_attr_int(child, "id", &tile_id)) continue;
            if (tile_id < 0 || tile_id >= out_tileset->tilecount) continue;
            size_t tile_children = xml_node_children(child);
            for (size_t j = 0; j < tile_children; ++j) {
                struct xml_node *node_child = xml_node_child(child, j);
                if (tiled_node_name_is(node_child, "properties")) {
                    size_t prop_count = xml_node_children(node_child);
                    for (size_t p = 0; p < prop_count; ++p) {
                        struct xml_node *prop = xml_node_child(node_child, p);
                        if (!tiled_node_name_is(prop, "property")) continue;
                        char *pname = tiled_node_attr_strdup(prop, "name");
                        if (!pname) continue;
                        if (strcmp(pname, "collider") == 0) {
                            char *pval = tiled_node_attr_strdup(prop, "value");
                            bool ok = false;
                            uint16_t mask = parse_collider_mask(pval, &ok);
                            if (ok) out_tileset->colliders[tile_id] = mask;
                            free(pval);
                        } else if (strcmp(pname, "renderstyle") == 0) {
                            char *pval = tiled_node_attr_strdup(prop, "value");
                            if (pval && strcasecmp(pval, "painters") == 0) {
                                out_tileset->render_painters[tile_id] = true;
                            }
                            free(pval);
                        } else if (strcmp(pname, "painteroffset") == 0) {
                            char *pval = tiled_node_attr_strdup(prop, "value");
                            if (pval) {
                                out_tileset->painter_offset[tile_id] = atoi(pval);
                                free(pval);
                            }
                        } else if (strcmp(pname, "animationtype") == 0) {
                            char *pval = tiled_node_attr_strdup(prop, "value");
                            if (pval && strcasecmp(pval, "door") == 0) {
                                out_tileset->no_merge_collider[tile_id] = true;
                            }
                            free(pval);
                        }
                        free(pname);
                    }
                } else if (tiled_node_name_is(node_child, "animation")) {
                    size_t frame_count = 0;
                    size_t anim_children = xml_node_children(node_child);
                    for (size_t k = 0; k < anim_children; ++k) {
                        if (tiled_node_name_is(xml_node_child(node_child, k), "frame")) frame_count++;
                    }
                    if (frame_count > 0) {
                        tiled_anim_frame_t *frames = bump_alloc_type(&g_tile_anim_arena, tiled_anim_frame_t, frame_count);
                        if (!frames) {
                            free(out_tileset->colliders);
                            free(out_tileset->no_merge_collider);
                            tiled_free_tileset_anims(out_tileset);
                            free(out_tileset->render_painters);
                            free(out_tileset->painter_offset);
                            return false;
                        }
                        memset(frames, 0, frame_count * sizeof(tiled_anim_frame_t));
                        int total_ms = 0;
                        size_t fi = 0;
                        for (size_t k = 0; k < anim_children && fi < frame_count; ++k) {
                            struct xml_node *frame = xml_node_child(node_child, k);
                            if (!tiled_node_name_is(frame, "frame")) continue;
                            tiled_node_attr_int(frame, "tileid", &frames[fi].tile_id);
                            tiled_node_attr_int(frame, "duration", &frames[fi].duration_ms);
                            if (frames[fi].duration_ms < 0) frames[fi].duration_ms = 0;
                            total_ms += frames[fi].duration_ms;
                            fi++;
                        }
                        out_tileset->anims[tile_id].frames = frames;
                        out_tileset->anims[tile_id].frame_count = fi;
                        out_tileset->anims[tile_id].total_duration_ms = total_ms;

                        // Propagate door/no-merge flag to all frame tiles so runtime detection works on animated frames.
                        if (out_tileset->no_merge_collider && out_tileset->no_merge_collider[tile_id]) {
                            for (size_t f = 0; f < fi; ++f) {
                                int fid = frames[f].tile_id;
                                if (fid >= 0 && fid < out_tileset->tilecount) {
                                    out_tileset->no_merge_collider[fid] = true;
                                }
                            }
                        }
                        if (out_tileset->render_painters && out_tileset->render_painters[tile_id]) {
                            for (size_t f = 0; f < fi; ++f) {
                                int fid = frames[f].tile_id;
                                if (fid >= 0 && fid < out_tileset->tilecount) {
                                    out_tileset->render_painters[fid] = true;
                                    if (out_tileset->painter_offset) {
                                        out_tileset->painter_offset[fid] = out_tileset->painter_offset[tile_id];
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (!image) {
        LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Inline tileset has no <image>");
        free(out_tileset->colliders);
        free(out_tileset->no_merge_collider);
        tiled_free_tileset_anims(out_tileset);
        free(out_tileset->render_painters);
        free(out_tileset->painter_offset);
        return false;
    }

    char *img_rel = tiled_node_attr_strdup(image, "source");
    if (!img_rel) {
        img_rel = tiled_scan_attr_in_file(tmx_path, "<image", "source");
    }
    if (!img_rel) {
        LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Inline tileset image missing source");
        free(out_tileset->colliders);
        free(out_tileset->no_merge_collider);
        tiled_free_tileset_anims(out_tileset);
        free(out_tileset->render_painters);
        free(out_tileset->painter_offset);
        return false;
    }
    out_tileset->image_path = tiled_join_relative(tmx_path, img_rel);
    free(img_rel);

    if (!out_tileset->image_path || !tiled_file_exists(out_tileset->image_path)) {
        free(out_tileset->image_path);
        out_tileset->image_path = NULL;
        char *scanned = tiled_scan_attr_in_file(tmx_path, "<image", "source");
        if (scanned) {
            out_tileset->image_path = tiled_join_relative(tmx_path, scanned);
            free(scanned);
        }
    }

    tiled_node_attr_int(image, "width", &out_tileset->image_width);
    tiled_node_attr_int(image, "height", &out_tileset->image_height);

    if (!out_tileset->image_path) {
        LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Failed to resolve inline tileset image path");
        free(out_tileset->colliders);
        free(out_tileset->no_merge_collider);
        tiled_free_tileset_anims(out_tileset);
        free(out_tileset->render_painters);
        free(out_tileset->painter_offset);
        return false;
    }
    return true;
}

bool tiled_parse_tilesets_from_root(struct xml_node *root, const char *tmx_path, world_map_t *out_map) {
    size_t children = xml_node_children(root);

    size_t ts_cap = 2;
    out_map->tilesets = (tiled_tileset_t *)calloc(ts_cap, sizeof(tiled_tileset_t));
    if (!out_map->tilesets) {
        return false;
    }

    bool ok = true;
    for (size_t i = 0; i < children; ++i) {
        struct xml_node *child = xml_node_child(root, i);
        if (!tiled_node_name_is(child, "tileset")) continue;

        if (out_map->tileset_count == ts_cap) {
            ts_cap *= 2;
            tiled_tileset_t *tmp = (tiled_tileset_t *)realloc(out_map->tilesets, ts_cap * sizeof(tiled_tileset_t));
            if (!tmp) { ok = false; break; }
            out_map->tilesets = tmp;
        }

        tiled_tileset_t *ts = &out_map->tilesets[out_map->tileset_count];
        memset(ts, 0, sizeof(*ts));

        int first_gid = 0;
        if (!tiled_node_attr_int(child, "firstgid", &first_gid)) {
            LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Tileset missing firstgid");
            ok = false;
            break;
        }

        char *tsx_rel = tiled_node_attr_strdup(child, "source");
        bool tileset_ok = false;
        if (tsx_rel && tsx_rel[0] != '\0') {
            char *tsx_path = tiled_join_relative(tmx_path, tsx_rel);
            free(tsx_rel);
            if (!tsx_path) {
                LOGC(LOGCAT_TILE, LOG_LVL_ERROR, "Could not resolve TSX path");
            } else {
                if (parse_tileset(tsx_path, ts)) {
                    ts->first_gid = first_gid;
                    char *img_path = tiled_join_relative(tsx_path, ts->image_path);
                    free(ts->image_path);
                    ts->image_path = img_path;
                } else {
                    char *img_rel = tiled_scan_attr_in_file(tsx_path, "<image", "source");
                    if (img_rel) {
                        ts->image_path = tiled_join_relative(tsx_path, img_rel);
                        free(img_rel);
                    }
                }

                if (!ts->image_path || !tiled_file_exists(ts->image_path)) {
                    free(ts->image_path);
                    ts->image_path = NULL;
                    char *img_rel = tiled_scan_attr_in_file(tsx_path, "<image", "source");
                    if (img_rel) {
                        ts->image_path = tiled_join_relative(tsx_path, img_rel);
                        free(img_rel);
                    }
                }
                tileset_ok = ts->image_path != NULL;
                free(tsx_path);
            }
        } else {
            free(tsx_rel);
            tileset_ok = parse_tileset_inline(child, tmx_path, ts);
            if (tileset_ok) {
                ts->first_gid = first_gid;
            }
        }

        if (!tileset_ok) {
            ok = false;
            break;
        }
        out_map->tileset_count++;
    }

    if (!ok || out_map->tileset_count == 0) {
        return false;
    }
    return true;
}
