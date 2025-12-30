#include "modules/prefab/prefab.h"
#include "modules/ecs/components_meta.h"
#include "modules/core/logger.h"
#include "xml.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void free_component(prefab_component_t* c);

// Duplicate a C string
static char* pf_xstrdup(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static bool pf_parse_bool_str(const char* s)
{
    if (!s) return false;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return false;
    return (strcasecmp(s, "1") == 0) ||
           (strcasecmp(s, "true") == 0) ||
           (strcasecmp(s, "yes") == 0) ||
           (strcasecmp(s, "on") == 0);
}

// Duplicate an xml_string into a null-terminated C string
static char* xml_string_dup_local(struct xml_string* xs) {
    if (!xs) return NULL;
    size_t len = xml_string_length(xs);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    xml_string_copy(xs, (uint8_t*)buf, len);
    buf[len] = '\0';
    return buf;
}

// Load and parse an XML document from a file path
static struct xml_document* load_xml_document(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long len = ftell(f);
    if (len < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }

    size_t n = (size_t)len;
    char* buf = (char*)malloc(n + 1);
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
        char* pi_end = strstr(buf + offset, "?>");
        if (pi_end) {
            offset = (size_t)((pi_end - buf) + 2);
        }
    }

    size_t trimmed = (offset <= n) ? (n - offset) : 0;
    if (trimmed == 0) {
        free(buf);
        return NULL;
    }
    char* clean = (char*)malloc(trimmed);
    if (!clean) {
        free(buf);
        return NULL;
    }
    memcpy(clean, buf + offset, trimmed);
    free(buf);

    return xml_parse_document((uint8_t*)clean, trimmed);
}

// Get a duplicated attribute value by name from an XML node
static char* node_attr_strdup_local(struct xml_node* node, const char* name) {
    if (!node || !name) return NULL;
    size_t attr_count = xml_node_attributes(node);
    for (size_t i = 0; i < attr_count; ++i) {
        struct xml_string* an = xml_node_attribute_name(node, i);
        if (!an) continue;
        size_t len = xml_string_length(an);
        char* tmp = (char*)malloc(len + 1);
        if (!tmp) return NULL;
        xml_string_copy(an, (uint8_t*)tmp, len);
        tmp[len] = '\0';
        bool match = (strcmp(tmp, name) == 0);
        if (match) {
            struct xml_string* av = xml_node_attribute_content(node, i);
            char* out = xml_string_dup_local(av);
            free(tmp);
            return out;
        }
        free(tmp);
    }
    return NULL;
}

// Check if an XML node's name matches a given local name
static bool node_name_is_local(struct xml_node* node, const char* name) {
    struct xml_string* ns = xml_node_name(node);
    if (!ns || !name) return false;
    size_t len = xml_string_length(ns);
    char* tmp = (char*)malloc(len + 1);
    if (!tmp) return false;
    xml_string_copy(ns, (uint8_t*)tmp, len);
    tmp[len] = '\0';
    bool eq = strcmp(tmp, name) == 0;
    free(tmp);
    return eq;
}

// Add a property key-value pair to a prefab component
static void add_prop(prefab_component_t* comp, char* name, char* value) {
    if (!comp || !name) {
        free(name);
        free(value);
        return;
    }
    size_t idx = comp->prop_count;
    prefab_kv_t* tmp = (prefab_kv_t*)realloc(comp->props, (idx + 1) * sizeof(prefab_kv_t));
    if (!tmp) {
        free(name);
        free(value);
        return;
    }
    comp->props = tmp;
    comp->props[idx].name = name;
    comp->props[idx].value = value ? value : pf_xstrdup("");
    comp->prop_count = idx + 1;
}

// Parse an animation sequence from an XML node (bit weird here, the def is separate from other component data)
static bool parse_anim_sequence(struct xml_node* anim_node, prefab_anim_def_t* def) {
    if (!def) return false;
    prefab_anim_seq_t seq = {0};
    seq.name = node_attr_strdup_local(anim_node, "name");

    size_t child_count = xml_node_children(anim_node);
    size_t frame_count = 0;
    for (size_t i = 0; i < child_count; ++i) {
        struct xml_node* frame_node = xml_node_child(anim_node, i);
        if (node_name_is_local(frame_node, "frame")) frame_count++;
    }

    if (frame_count > 0) {
        seq.frames = (anim_frame_coord_t*)malloc(frame_count * sizeof(anim_frame_coord_t));
        if (!seq.frames) {
            free(seq.name);
            return false;
        }
    }

    size_t fi = 0;
    for (size_t i = 0; i < child_count && fi < frame_count; ++i) {
        struct xml_node* frame_node = xml_node_child(anim_node, i);
        if (!node_name_is_local(frame_node, "frame")) continue;
        int col = 0, row = 0;
        char* scol = node_attr_strdup_local(frame_node, "col");
        char* srow = node_attr_strdup_local(frame_node, "row");
        if (scol) { col = atoi(scol); free(scol); }
        if (srow) { row = atoi(srow); free(srow); }
        seq.frames[fi++] = (anim_frame_coord_t){ (uint8_t)col, (uint8_t)row };
    }
    seq.frame_count = fi;

    prefab_anim_seq_t* dst = (prefab_anim_seq_t*)realloc(def->seqs, (def->seq_count + 1) * sizeof(prefab_anim_seq_t));
    if (!dst) {
        free(seq.frames);
        free(seq.name);
        return false;
    }
    def->seqs = dst;
    def->seqs[def->seq_count++] = seq;
    return true;
}

// Parse an animation component from an XML node
static bool parse_anim_component(struct xml_node* node, prefab_component_t* out) {
    if (!out) return false;
    prefab_anim_def_t* def = (prefab_anim_def_t*)calloc(1, sizeof(prefab_anim_def_t));
    if (!def) return false;
    def->frame_w = 0;
    def->frame_h = 0;
    def->fps = 0.0f;

    char* fw = node_attr_strdup_local(node, "frame_w");
    char* fh = node_attr_strdup_local(node, "frame_h");
    char* ffps = node_attr_strdup_local(node, "fps");
    if (fw) { def->frame_w = atoi(fw); free(fw); }
    if (fh) { def->frame_h = atoi(fh); free(fh); }
    if (ffps) { def->fps = (float)atof(ffps); free(ffps); }

    size_t child_count = xml_node_children(node);
    for (size_t i = 0; i < child_count; ++i) {
        struct xml_node* child = xml_node_child(node, i);
        if (node_name_is_local(child, "anim")) {
            if (!parse_anim_sequence(child, def)) {
                free(def->seqs);
                free(def);
                return false;
            }
        }
    }

    out->anim = def;
    return true;
}

// Parse a component XML node into a prefab component structure
static bool parse_component_node(struct xml_node* comp_node, prefab_component_t* out) {
    memset(out, 0, sizeof(*out));
    char* type = node_attr_strdup_local(comp_node, "type");
    ComponentEnum id;
    if (!type || !component_id_from_string(type, &id)) {
        LOGC(LOGCAT_PREFAB, LOG_LVL_ERROR, "prefab: unknown component type");
        free(type);
        return false;
    }
    out->id = id;
    out->type_name = type;

    size_t attr_count = xml_node_attributes(comp_node);
    for (size_t i = 0; i < attr_count; ++i) {
        struct xml_string* an = xml_node_attribute_name(comp_node, i);
        struct xml_string* av = xml_node_attribute_content(comp_node, i);
        char* n = xml_string_dup_local(an);
        if (!n) continue;
        if (strcasecmp(n, "type") == 0) {
            free(n);
            continue;
        }
        if (strcasecmp(n, "override") == 0) {
            char* v = xml_string_dup_local(av);
            out->override_after_spawn = pf_parse_bool_str(v);
            free(v);
            free(n);
            continue;
        }
        char* v = xml_string_dup_local(av);
        add_prop(out, n, v);
    }

    size_t child_count = xml_node_children(comp_node);
    for (size_t i = 0; i < child_count; ++i) {
        struct xml_node* child = xml_node_child(comp_node, i);
        if (node_name_is_local(child, "property")) {
            char* pname = node_attr_strdup_local(child, "name");
            if (!pname) continue;
            char* pval = node_attr_strdup_local(child, "value");
            if (!pval) {
                struct xml_string* pv = xml_node_content(child);
                pval = xml_string_dup_local(pv);
            }
            add_prop(out, pname, pval ? pval : pf_xstrdup(""));
        }
    }
    // Special handling for animation component, ugly but whatever
    if (out->id == ENUM_ANIM && !out->anim) {
        if (!parse_anim_component(comp_node, out)) {
            free_component(out);
            return false;
        }
    }

    return true;
}

// Free memory allocated for a prefab component
static void free_component(prefab_component_t* c) {
    if (!c) return;
    free(c->type_name);
    for (size_t p = 0; p < c->prop_count; ++p) {
        free(c->props[p].name);
        free(c->props[p].value);
    }
    free(c->props);
    if (c->anim) {
        for (size_t a = 0; a < c->anim->seq_count; ++a) {
            free(c->anim->seqs[a].name);
            free(c->anim->seqs[a].frames);
        }
        free(c->anim->seqs);
        free(c->anim);
    }
    // dono why i have to do this
    *c = (prefab_component_t){0};
}

bool prefab_load(const char* path, prefab_t* out_prefab) {
    if (!path || !out_prefab) return false;
    *out_prefab = (prefab_t){0};

    struct xml_document* doc = load_xml_document(path);
    if (!doc) {
        LOGC(LOGCAT_PREFAB, LOG_LVL_ERROR, "prefab: failed to read %s", path);
        return false;
    }
    struct xml_node* root = xml_document_root(doc);
    if (!root || !node_name_is_local(root, "prefab")) {
        LOGC(LOGCAT_PREFAB, LOG_LVL_ERROR, "prefab: root must be <prefab> in %s", path);
        xml_document_free(doc, true);
        return false;
    }

    out_prefab->name = node_attr_strdup_local(root, "name");

    size_t children = xml_node_children(root);
    for (size_t i = 0; i < children; ++i) {
        struct xml_node* child = xml_node_child(root, i);
        if (!node_name_is_local(child, "component")) continue;

        prefab_component_t comp = {0};
        if (!parse_component_node(child, &comp)) {
            free_component(&comp);
            xml_document_free(doc, true);
            prefab_free(out_prefab);
            return false;
        }
        prefab_component_t* tmp = (prefab_component_t*)realloc(out_prefab->components, (out_prefab->component_count + 1) * sizeof(prefab_component_t));
        if (!tmp) {
            free_component(&comp);
            prefab_free(out_prefab);
            xml_document_free(doc, true);
            return false;
        }
        out_prefab->components = tmp;
        out_prefab->components[out_prefab->component_count++] = comp;
    }

    xml_document_free(doc, true);
    return true;
}

// Free memory allocated for a prefab
void prefab_free(prefab_t* prefab) {
    if (!prefab) return;
    free(prefab->name);
    //Free each component
    if (prefab->components) {
        for (size_t i = 0; i < prefab->component_count; ++i) {
            free_component(&prefab->components[i]);
        }
    }
    free(prefab->components);
    *prefab = (prefab_t){0};
}
