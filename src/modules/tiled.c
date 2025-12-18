#include "../includes/tiled.h"
#include "../includes/bump_alloc.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "xml.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *xml_string_dup(struct xml_string *xs) {
    if (!xs) return NULL;
    size_t len = xml_string_length(xs);
    char *buf = (char *)malloc(len + 1);
    if (!buf) return NULL;
    xml_string_copy(xs, (uint8_t *)buf, len);
    buf[len] = '\0';
    return buf;
}

static bool xml_string_equals(struct xml_string *xs, const char *lit) {
    if (!xs || !lit) return false;
    size_t len = xml_string_length(xs);
    size_t want = strlen(lit);
    if (len != want) return false;
    char *tmp = (char *)malloc(len + 1);
    if (!tmp) return false;
    xml_string_copy(xs, (uint8_t *)tmp, len);
    tmp[len] = '\0';
    bool eq = memcmp(tmp, lit, len) == 0;
    free(tmp);
    return eq;
}

static char *node_attr_strdup(struct xml_node *node, const char *name) {
    if (!node || !name) return NULL;
    size_t attr_count = xml_node_attributes(node);
    for (size_t i = 0; i < attr_count; ++i) {
        struct xml_string *an = xml_node_attribute_name(node, i);
        if (!xml_string_equals(an, name)) continue;
        struct xml_string *av = xml_node_attribute_content(node, i);
        return xml_string_dup(av);
    }
    return NULL;
}

static bool node_attr_int(struct xml_node *node, const char *name, int *out) {
    char *val = node_attr_strdup(node, name);
    if (!val) return false;
    *out = atoi(val);
    free(val);
    return true;
}

static bool node_name_is(struct xml_node *node, const char *name) {
    return xml_string_equals(xml_node_name(node), name);
}

static bool file_exists(const char *path) {
    if (!path || !*path) return false;
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

static bool parse_bool_str(const char *s) {
    if (!s) return false;
    if (*s == '\0') return true;
    if (strcasecmp(s, "true") == 0 || strcmp(s, "1") == 0 || strcasecmp(s, "yes") == 0 || strcasecmp(s, "on") == 0) return true;
    return false;
}

static bool str_ieq_local(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        ++a; ++b;
    }
    return *a == '\0' && *b == '\0';
}

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
    obj->properties[idx].value = value ? value : xstrdup("");
    obj->property_count = idx + 1;
}

static bump_alloc_t g_tile_anim_arena;
static const size_t TILE_ANIM_ARENA_BYTES = 512 * 1024;

static bool ensure_tile_anim_arena(void) {
    if (g_tile_anim_arena.data) return true;
    return bump_init(&g_tile_anim_arena, TILE_ANIM_ARENA_BYTES);
}

static void reset_tile_anim_arena(void) {
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

static void free_tileset_anims(tiled_tileset_t *ts) {
    if (!ts || !ts->anims) return;
    free(ts->anims);
    ts->anims = NULL;
}

static char *scan_attr_in_file(const char *path, const char *tag, const char *attr) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long len = ftell(f);
    if (len < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    size_t n = (size_t)len;
    char *buf = (char *)malloc(n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read = fread(buf, 1, n, f);
    fclose(f);
    buf[read] = '\0';

    char *p = strstr(buf, tag);
    if (!p) { free(buf); return NULL; }
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "%s=\"", attr);
    char *a = strstr(p, pattern);
    if (!a) { free(buf); return NULL; }
    a += strlen(pattern);
    char *end = strchr(a, '"');
    if (!end) { free(buf); return NULL; }
    size_t len_val = (size_t)(end - a);
    char *out = (char *)malloc(len_val + 1);
    if (!out) { free(buf); return NULL; }
    memcpy(out, a, len_val);
    out[len_val] = '\0';
    free(buf);
    return out;
}

static struct xml_document *load_xml_document(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long len = ftell(f);
    if (len < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }

    size_t n = (size_t)len;
    char *buf = (char *)malloc(n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read = fread(buf, 1, n, f);
    fclose(f);
    buf[read] = '\0';
    n = read;

    size_t offset = 0;
    if (n >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) {
        offset = 3;
    }
    if (n > offset + 5 && strncmp(buf + offset, "<?xml", 5) == 0) {
        char *pi_end = strstr(buf + offset, "?>");
        if (pi_end) {
            offset = (size_t)((pi_end - buf) + 2);
        }
    }

    size_t trimmed = (offset <= n) ? (n - offset) : 0;
    if (trimmed == 0) {
        free(buf);
        return NULL;
    }
    char *clean = (char *)malloc(trimmed);
    if (!clean) {
        free(buf);
        return NULL;
    }
    memcpy(clean, buf + offset, trimmed);
    free(buf);

    return xml_parse_document((uint8_t *)clean, trimmed);
}

// Normalize paths like "a/../b/./c" into "b/c" without touching the filesystem
static char *normalize_path(const char *path) {
    if (!path) return NULL;
    size_t len = strlen(path);
    char *tmp = (char *)malloc(len + 1);
    if (!tmp) return NULL;
    memcpy(tmp, path, len + 1);

    char **stack = (char **)malloc((len / 2 + 2) * sizeof(char *));
    if (!stack) {
        free(tmp);
        return NULL;
    }

    size_t sp = 0;
    bool absolute = len > 0 && (path[0] == '/' || path[0] == '\\');

    char *p = tmp;
    while (*p) {
        while (*p == '/' || *p == '\\') {
            *p = '\0';
            p++;
        }
        if (!*p) break;

        char *seg = p;
        while (*p && *p != '/' && *p != '\\') p++;
        if (*p) {
            *p = '\0';
            p++;
        }

        if (strcmp(seg, ".") == 0) continue;
        if (strcmp(seg, "..") == 0) {
            if (sp > 0 && strcmp(stack[sp - 1], "..") != 0) {
                sp--;
            } else if (!absolute) {
                stack[sp++] = seg;
            }
            continue;
        }

        stack[sp++] = seg;
    }

    if (sp == 0 && !absolute) {
        free(stack);
        free(tmp);
        char *out = (char *)malloc(2);
        if (out) {
            out[0] = '.';
            out[1] = '\0';
        }
        return out;
    }

    size_t out_len = absolute ? 1 : 0;
    for (size_t i = 0; i < sp; ++i) {
        out_len += strlen(stack[i]);
        if (i + 1 < sp) out_len += 1;
    }

    char *out = (char *)malloc(out_len + 1);
    if (!out) {
        free(stack);
        free(tmp);
        return NULL;
    }

    size_t pos = 0;
    if (absolute) out[pos++] = '/';
    for (size_t i = 0; i < sp; ++i) {
        size_t slen = strlen(stack[i]);
        memcpy(out + pos, stack[i], slen);
        pos += slen;
        if (i + 1 < sp) out[pos++] = '/';
    }
    out[pos] = '\0';

    free(stack);
    free(tmp);
    return out;
}

static char *join_relative(const char *base_path, const char *rel) {
    if (!rel || rel[0] == '\0') return NULL;
    if (rel[0] == '/' || rel[0] == '\\') return normalize_path(rel);
    const char *slash = strrchr(base_path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(base_path, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    if (!slash) return normalize_path(rel);
    size_t dir_len = (size_t)(slash - base_path) + 1;
    size_t rel_len = strlen(rel);
    char *buf = (char *)malloc(dir_len + rel_len + 1);
    if (!buf) return NULL;
    memcpy(buf, base_path, dir_len);
    memcpy(buf + dir_len, rel, rel_len);
    buf[dir_len + rel_len] = '\0';
    char *out = normalize_path(buf);
    free(buf);
    return out;
}

static bool parse_csv_gids(const char *csv, size_t expected, uint32_t *out) {
    if (!csv || !out) return false;
    size_t count = 0;
    const char *p = csv;
    while (*p && count < expected) {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (!*p) break;
        char *end = NULL;
        long v = strtol(p, &end, 10);
        if (p == end) break;
        if (v < 0) v = 0;
        out[count++] = (uint32_t)v;
        p = end;
    }
    return count == expected;
}

static bool parse_tileset(const char *tsx_path, tiled_tileset_t *out_tileset) {
    memset(out_tileset, 0, sizeof(*out_tileset));
    struct xml_document *doc = load_xml_document(tsx_path);
    if (!doc) {
        fprintf(stderr, "Failed to parse TSX: %s\n", tsx_path);
        return false;
    }
    struct xml_node *root = xml_document_root(doc);
    if (!root || !node_name_is(root, "tileset")) {
        fprintf(stderr, "Unexpected TSX root\n");
        xml_document_free(doc, true);
        return false;
    }

    if (!node_attr_int(root, "tilewidth", &out_tileset->tilewidth) ||
        !node_attr_int(root, "tileheight", &out_tileset->tileheight) ||
        !node_attr_int(root, "tilecount", &out_tileset->tilecount) ||
        !node_attr_int(root, "columns", &out_tileset->columns)) {
        fprintf(stderr, "Missing tileset attributes\n");
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
        if (node_name_is(child, "image")) {
            image = child;
        } else if (node_name_is(child, "tile")) {
            int tile_id = 0;
            if (!node_attr_int(child, "id", &tile_id)) continue;
            if (tile_id < 0 || tile_id >= out_tileset->tilecount) continue;
            size_t tile_children = xml_node_children(child);
            for (size_t j = 0; j < tile_children; ++j) {
                struct xml_node *node_child = xml_node_child(child, j);
                if (node_name_is(node_child, "properties")) {
                    size_t prop_count = xml_node_children(node_child);
                    for (size_t p = 0; p < prop_count; ++p) {
                        struct xml_node *prop = xml_node_child(node_child, p);
                        if (!node_name_is(prop, "property")) continue;
                        char *pname = node_attr_strdup(prop, "name");
                        if (!pname) continue;
                        if (strcmp(pname, "collider") == 0) {
                            char *pval = node_attr_strdup(prop, "value");
                            bool ok = false;
                            uint16_t mask = parse_collider_mask(pval, &ok);
                            if (ok) out_tileset->colliders[tile_id] = mask;
                            free(pval);
                        } else if (strcmp(pname, "renderstyle") == 0) {
                            char *pval = node_attr_strdup(prop, "value");
                            if (pval && strcasecmp(pval, "painters") == 0) {
                                out_tileset->render_painters[tile_id] = true;
                            }
                            free(pval);
                        } else if (strcmp(pname, "painteroffset") == 0) {
                            char *pval = node_attr_strdup(prop, "value");
                            if (pval) {
                                out_tileset->painter_offset[tile_id] = atoi(pval);
                                free(pval);
                            }
                        } else if (strcmp(pname, "animationtype") == 0) {
                            char *pval = node_attr_strdup(prop, "value");
                            if (pval && strcasecmp(pval, "door") == 0) {
                                out_tileset->no_merge_collider[tile_id] = true;
                            }
                            free(pval);
                        }
                        free(pname);
                    }
                } else if (node_name_is(node_child, "animation")) {
                    size_t frame_count = 0;
                    size_t anim_children = xml_node_children(node_child);
                    for (size_t k = 0; k < anim_children; ++k) {
                        if (node_name_is(xml_node_child(node_child, k), "frame")) frame_count++;
                    }
                        if (frame_count > 0) {
                            tiled_anim_frame_t *frames = bump_alloc_type(&g_tile_anim_arena, tiled_anim_frame_t, frame_count);
                            if (!frames) {
                                free(out_tileset->colliders);
                                free(out_tileset->no_merge_collider);
                                free_tileset_anims(out_tileset);
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
                            if (!node_name_is(frame, "frame")) continue;
                            node_attr_int(frame, "tileid", &frames[fi].tile_id);
                            node_attr_int(frame, "duration", &frames[fi].duration_ms);
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
        fprintf(stderr, "No <image> in TSX\n");
        free(out_tileset->colliders);
        free(out_tileset->no_merge_collider);
        free_tileset_anims(out_tileset);
        free(out_tileset->render_painters);
        free(out_tileset->painter_offset);
        xml_document_free(doc, true);
        return false;
    }

    out_tileset->image_path = node_attr_strdup(image, "source");
    node_attr_int(image, "width", &out_tileset->image_width);
    node_attr_int(image, "height", &out_tileset->image_height);

    xml_document_free(doc, true);

    if (!out_tileset->image_path) {
        out_tileset->image_path = scan_attr_in_file(tsx_path, "<image", "source");
    }
    if (!out_tileset->image_path) {
        fprintf(stderr, "Tileset missing image source\n");
        free(out_tileset->colliders);
        free(out_tileset->no_merge_collider);
        free_tileset_anims(out_tileset);
        free(out_tileset->render_painters);
        free(out_tileset->painter_offset);
        return false;
    }
    return true;
}

static bool parse_tileset_inline(struct xml_node *tileset_node, const char *tmx_path, tiled_tileset_t *out_tileset) {
    memset(out_tileset, 0, sizeof(*out_tileset));
    if (!node_attr_int(tileset_node, "tilewidth", &out_tileset->tilewidth) ||
        !node_attr_int(tileset_node, "tileheight", &out_tileset->tileheight) ||
        !node_attr_int(tileset_node, "tilecount", &out_tileset->tilecount) ||
        !node_attr_int(tileset_node, "columns", &out_tileset->columns)) {
        fprintf(stderr, "Inline tileset missing attributes\n");
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
        if (node_name_is(child, "image")) {
            image = child;
        } else if (node_name_is(child, "tile")) {
            int tile_id = 0;
            if (!node_attr_int(child, "id", &tile_id)) continue;
            if (tile_id < 0 || tile_id >= out_tileset->tilecount) continue;
            size_t tile_children = xml_node_children(child);
            for (size_t j = 0; j < tile_children; ++j) {
                struct xml_node *node_child = xml_node_child(child, j);
                if (node_name_is(node_child, "properties")) {
                    size_t prop_count = xml_node_children(node_child);
                    for (size_t p = 0; p < prop_count; ++p) {
                        struct xml_node *prop = xml_node_child(node_child, p);
                        if (!node_name_is(prop, "property")) continue;
                        char *pname = node_attr_strdup(prop, "name");
                        if (!pname) continue;
                        if (strcmp(pname, "collider") == 0) {
                            char *pval = node_attr_strdup(prop, "value");
                            bool ok = false;
                            uint16_t mask = parse_collider_mask(pval, &ok);
                            if (ok) out_tileset->colliders[tile_id] = mask;
                            free(pval);
                        } else if (strcmp(pname, "renderstyle") == 0) {
                            char *pval = node_attr_strdup(prop, "value");
                            if (pval && strcasecmp(pval, "painters") == 0) {
                                out_tileset->render_painters[tile_id] = true;
                            }
                            free(pval);
                        } else if (strcmp(pname, "painteroffset") == 0) {
                            char *pval = node_attr_strdup(prop, "value");
                            if (pval) {
                                out_tileset->painter_offset[tile_id] = atoi(pval);
                                free(pval);
                            }
                        } else if (strcmp(pname, "animationtype") == 0) {
                            char *pval = node_attr_strdup(prop, "value");
                            if (pval && strcasecmp(pval, "door") == 0) {
                                out_tileset->no_merge_collider[tile_id] = true;
                            }
                            free(pval);
                        }
                        free(pname);
                    }
                } else if (node_name_is(node_child, "animation")) {
                    size_t frame_count = 0;
                    size_t anim_children = xml_node_children(node_child);
                    for (size_t k = 0; k < anim_children; ++k) {
                        if (node_name_is(xml_node_child(node_child, k), "frame")) frame_count++;
                    }
                    if (frame_count > 0) {
                        tiled_anim_frame_t *frames = bump_alloc_type(&g_tile_anim_arena, tiled_anim_frame_t, frame_count);
                        if (!frames) {
                            free(out_tileset->colliders);
                            free(out_tileset->no_merge_collider);
                            free_tileset_anims(out_tileset);
                            free(out_tileset->render_painters);
                            free(out_tileset->painter_offset);
                            return false;
                        }
                        memset(frames, 0, frame_count * sizeof(tiled_anim_frame_t));
                        int total_ms = 0;
                        size_t fi = 0;
                        for (size_t k = 0; k < anim_children && fi < frame_count; ++k) {
                            struct xml_node *frame = xml_node_child(node_child, k);
                            if (!node_name_is(frame, "frame")) continue;
                            node_attr_int(frame, "tileid", &frames[fi].tile_id);
                            node_attr_int(frame, "duration", &frames[fi].duration_ms);
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
    fprintf(stderr, "Inline tileset has no <image>\n");
    free(out_tileset->colliders);
    free(out_tileset->no_merge_collider);
    free_tileset_anims(out_tileset);
    free(out_tileset->render_painters);
    free(out_tileset->painter_offset);
    return false;
}

    char *img_rel = node_attr_strdup(image, "source");
    if (!img_rel) {
        img_rel = scan_attr_in_file(tmx_path, "<image", "source");
    }
if (!img_rel) {
    fprintf(stderr, "Inline tileset image missing source\n");
    free(out_tileset->colliders);
    free(out_tileset->no_merge_collider);
    free_tileset_anims(out_tileset);
    free(out_tileset->render_painters);
    free(out_tileset->painter_offset);
    return false;
}
    out_tileset->image_path = join_relative(tmx_path, img_rel);
    free(img_rel);

    if (!out_tileset->image_path || !file_exists(out_tileset->image_path)) {
        free(out_tileset->image_path);
        out_tileset->image_path = NULL;
        char *scanned = scan_attr_in_file(tmx_path, "<image", "source");
        if (scanned) {
            out_tileset->image_path = join_relative(tmx_path, scanned);
            free(scanned);
        }
    }

    node_attr_int(image, "width", &out_tileset->image_width);
    node_attr_int(image, "height", &out_tileset->image_height);

if (!out_tileset->image_path) {
    fprintf(stderr, "Failed to resolve inline tileset image path\n");
    free(out_tileset->colliders);
    free(out_tileset->no_merge_collider);
    free_tileset_anims(out_tileset);
    free(out_tileset->render_painters);
    free(out_tileset->painter_offset);
    return false;
}
    return true;
}

static bool parse_layer(struct xml_node *layer_node, tiled_layer_t *out_layer) {
    memset(out_layer, 0, sizeof(*out_layer));
    out_layer->name = node_attr_strdup(layer_node, "name");
    if (!node_attr_int(layer_node, "width", &out_layer->width) ||
        !node_attr_int(layer_node, "height", &out_layer->height)) {
        fprintf(stderr, "Layer missing width/height\n");
        return false;
    }

    size_t child_count = xml_node_children(layer_node);
    for (size_t i = 0; i < child_count; ++i) {
        struct xml_node *child = xml_node_child(layer_node, i);
        if (!node_name_is(child, "properties")) continue;
        size_t prop_count = xml_node_children(child);
        for (size_t p = 0; p < prop_count; ++p) {
            struct xml_node *prop = xml_node_child(child, p);
            if (!node_name_is(prop, "property")) continue;
            char *pname = node_attr_strdup(prop, "name");
            if (!pname) continue;
            if (strcmp(pname, "collision") == 0) {
                char *pval = node_attr_strdup(prop, "value");
                out_layer->collision = parse_bool_str(pval);
                free(pval);
            }
            free(pname);
        }
    }

    struct xml_node *data_node = NULL;
    for (size_t i = 0; i < child_count; ++i) {
        struct xml_node *child = xml_node_child(layer_node, i);
        if (node_name_is(child, "data")) {
            data_node = child;
            break;
        }
    }
    if (!data_node) {
        fprintf(stderr, "Layer missing <data>\n");
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
        if (node_name_is(xml_node_child(data_node, i), "chunk")) {
            has_chunk = true;
            break;
        }
    }

    bool ok = true;
    if (has_chunk) {
        for (size_t i = 0; i < data_children; ++i) {
            struct xml_node *chunk = xml_node_child(data_node, i);
            if (!node_name_is(chunk, "chunk")) continue;

            int cx = 0, cy = 0, cw = 0, ch = 0;
            if (!node_attr_int(chunk, "x", &cx) ||
                !node_attr_int(chunk, "y", &cy) ||
                !node_attr_int(chunk, "width", &cw) ||
                !node_attr_int(chunk, "height", &ch)) {
                fprintf(stderr, "Chunk missing position/size in layer '%s'\n", out_layer->name ? out_layer->name : "(unnamed)");
                ok = false;
                break;
            }

            struct xml_string *chunk_str = xml_node_content(chunk);
            char *csv = xml_string_dup(chunk_str);
            if (!csv) {
                fprintf(stderr, "Failed to copy chunk CSV for layer '%s'\n", out_layer->name ? out_layer->name : "(unnamed)");
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
            bool parsed_chunk = parse_csv_gids(csv, chunk_total, chunk_gids);
            free(csv);
            if (!parsed_chunk) {
                free(chunk_gids);
                fprintf(stderr, "CSV parse mismatch for chunk in layer '%s'\n", out_layer->name ? out_layer->name : "(unnamed)");
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
        char *csv = xml_string_dup(data_str);
        if (!csv) {
            fprintf(stderr, "Failed to copy layer CSV\n");
            ok = false;
        }
        if (ok) {
            bool parsed = parse_csv_gids(csv, total, out_layer->gids);
            free(csv);
            if (!parsed) {
                fprintf(stderr, "CSV parse mismatch for layer '%s'\n", out_layer->name ? out_layer->name : "(unnamed)");
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

static void free_objects(tiled_map_t *map) {
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
    node_attr_int(obj_node, "id", &out->id);
    char* gid_str = node_attr_strdup(obj_node, "gid");
    if (gid_str) {
        out->gid = (uint32_t)strtoul(gid_str, NULL, 10);
        free(gid_str);
    }
    out->name = node_attr_strdup(obj_node, "name");
    out->layer_name = layer_name ? xstrdup(layer_name) : NULL;
    out->layer_z = layer_z;
    char *x = node_attr_strdup(obj_node, "x");
    char *y = node_attr_strdup(obj_node, "y");
    char *w = node_attr_strdup(obj_node, "width");
    char *h = node_attr_strdup(obj_node, "height");
    if (x) { out->x = (float)atof(x); free(x); }
    if (y) { out->y = (float)atof(y); free(y); }
    if (w) { out->w = (float)atof(w); free(w); }
    if (h) { out->h = (float)atof(h); free(h); }

    size_t child_count = xml_node_children(obj_node);
    for (size_t i = 0; i < child_count; ++i) {
        struct xml_node *child = xml_node_child(obj_node, i);
        if (!node_name_is(child, "properties")) continue;
        size_t prop_count = xml_node_children(child);
        for (size_t p = 0; p < prop_count; ++p) {
            struct xml_node *prop = xml_node_child(child, p);
            if (!node_name_is(prop, "property")) continue;
            char *pname = node_attr_strdup(prop, "name");
            if (!pname) continue;
            char *pval = node_attr_strdup(prop, "value");
            if (!pval) {
                struct xml_string *pv = xml_node_content(prop);
                pval = xml_string_dup(pv);
            }
            char *ptype = node_attr_strdup(prop, "type");
            if (!pval) pval = xstrdup("");
            if (strcmp(pname, "animationtype") == 0) {
                out->animationtype = xstrdup(pval);
            } else if (strcmp(pname, "proximity_radius") == 0 && pval) {
                out->proximity_radius = atoi(pval);
            } else if (strcmp(pname, "proximity_offset") == 0 && pval) {
                int ox = 0, oy = 0;
                sscanf(pval, "%d,%d", &ox, &oy);
                out->proximity_off_x = ox;
                out->proximity_off_y = oy;
            } else if (strcmp(pname, "door_tiles") == 0 && pval) {
                // format: "x1,y1;x2,y2"
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

bool tiled_load_map(const char *tmx_path, tiled_map_t *out_map) {
    if (!out_map) return false;
    *out_map = (tiled_map_t){0};
    reset_tile_anim_arena();

    struct xml_document *doc = load_xml_document(tmx_path);
    if (!doc) {
        fprintf(stderr, "Failed to parse TMX: %s\n", tmx_path);
        return false;
    }
    struct xml_node *root = xml_document_root(doc);
    if (!root || !node_name_is(root, "map")) {
        fprintf(stderr, "Unexpected TMX root\n");
        xml_document_free(doc, true);
        return false;
    }

    if (!node_attr_int(root, "width", &out_map->width) ||
        !node_attr_int(root, "height", &out_map->height) ||
        !node_attr_int(root, "tilewidth", &out_map->tilewidth) ||
        !node_attr_int(root, "tileheight", &out_map->tileheight)) {
        fprintf(stderr, "Map missing required attributes\n");
        xml_document_free(doc, true);
        return false;
    }

    size_t children = xml_node_children(root);

    // Tilesets (support multiple)
    size_t ts_cap = 2;
    out_map->tilesets = (tiled_tileset_t *)calloc(ts_cap, sizeof(tiled_tileset_t));
    if (!out_map->tilesets) {
        xml_document_free(doc, true);
        return false;
    }

    bool ok = true;
    for (size_t i = 0; i < children; ++i) {
        struct xml_node *child = xml_node_child(root, i);
        if (!node_name_is(child, "tileset")) continue;

        if (out_map->tileset_count == ts_cap) {
            ts_cap *= 2;
            tiled_tileset_t *tmp = (tiled_tileset_t *)realloc(out_map->tilesets, ts_cap * sizeof(tiled_tileset_t));
            if (!tmp) { ok = false; break; }
            out_map->tilesets = tmp;
        }

        tiled_tileset_t *ts = &out_map->tilesets[out_map->tileset_count];
        memset(ts, 0, sizeof(*ts));

        int first_gid = 0;
        if (!node_attr_int(child, "firstgid", &first_gid)) {
            fprintf(stderr, "Tileset missing firstgid\n");
            ok = false;
            break;
        }

        char *tsx_rel = node_attr_strdup(child, "source");
        bool tileset_ok = false;
        if (tsx_rel && tsx_rel[0] != '\0') {
            char *tsx_path = join_relative(tmx_path, tsx_rel);
            free(tsx_rel);
            if (!tsx_path) {
                fprintf(stderr, "Could not resolve TSX path\n");
            } else {
                if (parse_tileset(tsx_path, ts)) {
                    ts->first_gid = first_gid;
                    char *img_path = join_relative(tsx_path, ts->image_path);
                    free(ts->image_path);
                    ts->image_path = img_path;
                } else {
                    char *img_rel = scan_attr_in_file(tsx_path, "<image", "source");
                    if (img_rel) {
                        ts->image_path = join_relative(tsx_path, img_rel);
                        free(img_rel);
                    }
                }

                if (!ts->image_path || !file_exists(ts->image_path)) {
                    free(ts->image_path);
                    ts->image_path = NULL;
                    char *img_rel = scan_attr_in_file(tsx_path, "<image", "source");
                    if (img_rel) {
                        ts->image_path = join_relative(tsx_path, img_rel);
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
        xml_document_free(doc, true);
        tiled_free_map(out_map);
        return false;
    }

    // Objects
    out_map->objects = NULL;
    out_map->object_count = 0;
    size_t obj_cap = 0;
    for (size_t i = 0; i < children; ++i) {
        struct xml_node *child = xml_node_child(root, i);
        if (!node_name_is(child, "objectgroup")) continue;
        size_t obj_children = xml_node_children(child);
        for (size_t j = 0; j < obj_children; ++j) {
            struct xml_node *obj = xml_node_child(child, j);
            if (!node_name_is(obj, "object")) continue;
            if (out_map->object_count == obj_cap) {
                obj_cap = obj_cap ? obj_cap * 2 : 8;
                tiled_object_t *tmp = (tiled_object_t *)realloc(out_map->objects, obj_cap * sizeof(tiled_object_t));
                if (!tmp) { ok = false; break; }
                out_map->objects = tmp;
            }
            char *layer_name = node_attr_strdup(child, "name");
            parse_object(obj, layer_name, (int)i, &out_map->objects[out_map->object_count]);
            out_map->object_count++;
            free(layer_name);
        }
        if (!ok) break;
    }

    // Layers
    size_t layer_cap = 4;
    out_map->layers = (tiled_layer_t *)calloc(layer_cap, sizeof(tiled_layer_t));
    if (!out_map->layers) {
        xml_document_free(doc, true);
        tiled_free_map(out_map);
        return false;
    }

    for (size_t i = 0; i < children; ++i) {
        struct xml_node *child = xml_node_child(root, i);
        if (!node_name_is(child, "layer")) continue;
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

    xml_document_free(doc, true);

    if (!ok) {
        tiled_free_map(out_map);
        return false;
    }
    return true;
}

const tiled_property_t* tiled_object_get_property(const tiled_object_t* obj, const char* name) {
    if (!obj || !name) return NULL;
    for (size_t i = 0; i < obj->property_count; ++i) {
        const tiled_property_t* p = &obj->properties[i];
        if (p->name && str_ieq_local(p->name, name)) {
            return p;
        }
    }
    return NULL;
}

const char* tiled_object_get_property_value(const tiled_object_t* obj, const char* name) {
    const tiled_property_t* p = tiled_object_get_property(obj, name);
    return p ? p->value : NULL;
}

void tiled_free_map(tiled_map_t *map) {
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
            free_tileset_anims(ts);
            free(ts->render_painters);
            free(ts->painter_offset);
            free(ts->image_path);
        }
    }
    free(map->tilesets);
    free_objects(map);
    *map = (tiled_map_t){0};
    bump_reset(&g_tile_anim_arena);
}
