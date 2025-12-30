#include "modules/tiled/tiled_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

char *tiled_xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

char *tiled_xml_string_dup(struct xml_string *xs) {
    if (!xs) return NULL;
    size_t len = xml_string_length(xs);
    char *buf = (char *)malloc(len + 1);
    if (!buf) return NULL;
    xml_string_copy(xs, (uint8_t *)buf, len);
    buf[len] = '\0';
    return buf;
}

static bool tiled_xml_string_equals(struct xml_string *xs, const char *lit) {
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

char *tiled_node_attr_strdup(struct xml_node *node, const char *name) {
    if (!node || !name) return NULL;
    size_t attr_count = xml_node_attributes(node);
    for (size_t i = 0; i < attr_count; ++i) {
        struct xml_string *an = xml_node_attribute_name(node, i);
        if (!tiled_xml_string_equals(an, name)) continue;
        struct xml_string *av = xml_node_attribute_content(node, i);
        return tiled_xml_string_dup(av);
    }
    return NULL;
}

bool tiled_node_attr_int(struct xml_node *node, const char *name, int *out) {
    char *val = tiled_node_attr_strdup(node, name);
    if (!val) return false;
    *out = atoi(val);
    free(val);
    return true;
}

bool tiled_node_name_is(struct xml_node *node, const char *name) {
    return tiled_xml_string_equals(xml_node_name(node), name);
}

bool tiled_file_exists(const char *path) {
    if (!path || !*path) return false;
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

bool tiled_parse_bool_str(const char *s) {
    if (!s) return false;
    if (*s == '\0') return true;
    if (strcasecmp(s, "true") == 0 || strcmp(s, "1") == 0 || strcasecmp(s, "yes") == 0 || strcasecmp(s, "on") == 0) return true;
    return false;
}

bool tiled_str_ieq(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        ++a; ++b;
    }
    return *a == '\0' && *b == '\0';
}

char *tiled_scan_attr_in_file(const char *path, const char *tag, const char *attr) {
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

struct xml_document *tiled_load_xml_document(const char *path) {
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

static char *tiled_normalize_path(const char *path) {
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

char *tiled_join_relative(const char *base_path, const char *rel) {
    if (!rel || rel[0] == '\0') return NULL;
    if (rel[0] == '/' || rel[0] == '\\') return tiled_normalize_path(rel);
    const char *slash = strrchr(base_path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(base_path, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    if (!slash) return tiled_normalize_path(rel);
    size_t dir_len = (size_t)(slash - base_path) + 1;
    size_t rel_len = strlen(rel);
    char *buf = (char *)malloc(dir_len + rel_len + 1);
    if (!buf) return NULL;
    memcpy(buf, base_path, dir_len);
    memcpy(buf + dir_len, rel, rel_len);
    buf[dir_len + rel_len] = '\0';
    char *out = tiled_normalize_path(buf);
    free(buf);
    return out;
}

bool tiled_parse_csv_gids(const char *csv, size_t expected, uint32_t *out) {
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
