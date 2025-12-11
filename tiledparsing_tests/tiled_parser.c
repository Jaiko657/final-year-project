#include "tiled_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../third_party/xml.c/src/xml.h"

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

static char *join_relative(const char *base_path, const char *rel) {
    if (!rel || rel[0] == '\0') return NULL;
    if (rel[0] == '/' || rel[0] == '\\') return xstrdup(rel);
    const char *slash = strrchr(base_path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(base_path, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    if (!slash) return xstrdup(rel);
    size_t dir_len = (size_t)(slash - base_path) + 1;
    size_t rel_len = strlen(rel);
    char *buf = (char *)malloc(dir_len + rel_len + 1);
    if (!buf) return NULL;
    memcpy(buf, base_path, dir_len);
    memcpy(buf + dir_len, rel, rel_len);
    buf[dir_len + rel_len] = '\0';
    return buf;
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

    struct xml_node *image = NULL;
    size_t children = xml_node_children(root);
    for (size_t i = 0; i < children; ++i) {
        struct xml_node *child = xml_node_child(root, i);
        if (node_name_is(child, "image")) {
            image = child;
            break;
        }
    }
    if (!image) {
        fprintf(stderr, "No <image> in TSX\n");
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

    struct xml_node *image = NULL;
    size_t children = xml_node_children(tileset_node);
    for (size_t i = 0; i < children; ++i) {
        struct xml_node *child = xml_node_child(tileset_node, i);
        if (node_name_is(child, "image")) {
            image = child;
            break;
        }
    }
    if (!image) {
        fprintf(stderr, "Inline tileset has no <image>\n");
        return false;
    }

    char *img_rel = node_attr_strdup(image, "source");
    if (!img_rel) {
        img_rel = scan_attr_in_file(tmx_path, "<image", "source");
    }
    if (!img_rel) {
        fprintf(stderr, "Inline tileset image missing source\n");
        return false;
    }
    out_tileset->image_path = join_relative(tmx_path, img_rel);
    free(img_rel);

    // xml.c seems to stop attributes at whitespace, so re-scan from the raw file
    // if the resolved path doesn't point to an existing texture.
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

    struct xml_node *data_node = NULL;
    size_t child_count = xml_node_children(layer_node);
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
                    if ((cx + x) < 0 || (cy + y) < 0 || di >= total) continue; // skip out-of-bounds chunks
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

bool tiled_load_map(const char *tmx_path, tiled_map_t *out_map) {
    memset(out_map, 0, sizeof(*out_map));
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

    // Tileset (pick the first one; ignore additional for this demo)
    struct xml_node *tileset_node = NULL;
    size_t children = xml_node_children(root);
    for (size_t i = 0; i < children; ++i) {
        struct xml_node *child = xml_node_child(root, i);
        if (node_name_is(child, "tileset")) {
            tileset_node = child;
            break;
        }
    }
    if (!tileset_node) {
        fprintf(stderr, "No <tileset> in TMX\n");
        xml_document_free(doc, true);
        return false;
    }
    if (!node_attr_int(tileset_node, "firstgid", &out_map->tileset.first_gid)) {
        fprintf(stderr, "Tileset missing firstgid\n");
        xml_document_free(doc, true);
        return false;
    }
    int map_first_gid = out_map->tileset.first_gid;
    char *tsx_rel = node_attr_strdup(tileset_node, "source");
    bool tileset_ok = false;
    if (tsx_rel && tsx_rel[0] != '\0') {
        char *tsx_path = join_relative(tmx_path, tsx_rel);
        free(tsx_rel);
        if (!tsx_path) {
            fprintf(stderr, "Could not resolve TSX path\n");
            xml_document_free(doc, true);
            return false;
        }
        if (parse_tileset(tsx_path, &out_map->tileset)) {
            out_map->tileset.first_gid = map_first_gid;
            // Fix image path to be relative to TMX directory as well
            char *img_path = join_relative(tsx_path, out_map->tileset.image_path);
            free(out_map->tileset.image_path);
            out_map->tileset.image_path = img_path;
        } else {
            char *img_rel = scan_attr_in_file(tsx_path, "<image", "source");
            if (img_rel) {
                out_map->tileset.image_path = join_relative(tsx_path, img_rel);
                free(img_rel);
            }
        }

        if (!out_map->tileset.image_path || !file_exists(out_map->tileset.image_path)) {
            free(out_map->tileset.image_path);
            out_map->tileset.image_path = NULL;
            char *img_rel = scan_attr_in_file(tsx_path, "<image", "source");
            if (img_rel) {
                out_map->tileset.image_path = join_relative(tsx_path, img_rel);
                free(img_rel);
            }
        }
        tileset_ok = out_map->tileset.image_path != NULL;
        free(tsx_path);
    } else {
        free(tsx_rel);
        tileset_ok = parse_tileset_inline(tileset_node, tmx_path, &out_map->tileset);
        if (tileset_ok) {
            out_map->tileset.first_gid = map_first_gid;
        }
    }
    if (!tileset_ok) {
        xml_document_free(doc, true);
        return false;
    }

    // Layers
    size_t layer_cap = 4;
    tiled_layer_t *layers = (tiled_layer_t *)calloc(layer_cap, sizeof(tiled_layer_t));
    if (!layers) {
        xml_document_free(doc, true);
        return false;
    }

    size_t layer_count = 0;
    bool ok = true;
    for (size_t i = 0; i < children; ++i) {
        struct xml_node *child = xml_node_child(root, i);
        if (!node_name_is(child, "layer")) continue;
        if (layer_count == layer_cap) {
            layer_cap *= 2;
            tiled_layer_t *tmp = (tiled_layer_t *)realloc(layers, layer_cap * sizeof(tiled_layer_t));
            if (!tmp) {
                ok = false;
                break;
            }
            layers = tmp;
        }
        if (!parse_layer(child, &layers[layer_count])) {
            ok = false;
            break;
        }
        layer_count++;
    }

    xml_document_free(doc, true);

    if (!ok) {
        tiled_map_t cleanup = {0};
        cleanup.layers = layers;
        cleanup.layer_count = layer_count;
        cleanup.tileset.image_path = out_map->tileset.image_path;
        tiled_free_map(&cleanup);
        out_map->tileset.image_path = NULL;
        return false;
    }

    out_map->layers = layers;
    out_map->layer_count = layer_count;
    return true;
}

void tiled_free_map(tiled_map_t *map) {
    if (!map) return;
    for (size_t i = 0; i < map->layer_count; ++i) {
        tiled_layer_t *l = &map->layers[i];
        free(l->name);
        free(l->gids);
    }
    free(map->layers);
    free(map->tileset.image_path);
    *map = (tiled_map_t){0};
}
