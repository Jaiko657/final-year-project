#include "../includes/prefab.h"
#include "../includes/logger.h"
#include "../includes/asset.h"
#include "../includes/ecs_internal.h"
#include "../includes/ecs_game.h"
#include "xml.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    char* name;
    char* value;
} prefab_kv_t;

typedef struct {
    char* name;
    anim_frame_coord_t* frames;
    size_t frame_count;
} prefab_anim_seq_t;

typedef struct {
    int frame_w;
    int frame_h;
    float fps;
    prefab_anim_seq_t* seqs;
    size_t seq_count;
} prefab_anim_def_t;

struct prefab_component_t {
    ComponentEnum id;
    char* type_name;
    prefab_kv_t* props;
    size_t prop_count;
    prefab_anim_def_t* anim; // only for ENUM_ANIM
};

struct prefab_t {
    char* name;
    prefab_component_t* components;
    size_t component_count;
};

static void free_component(prefab_component_t* c);

typedef struct {
    const char* name;
    ComponentEnum id;
} comp_name_map_t;

typedef struct {
    const char* name;
    uint32_t mask;
} comp_mask_map_t;

static const comp_name_map_t k_comp_names[] = {
#define X(name) { #name, ENUM_##name },
    #include "../includes/components.def"
#undef X
};
static const comp_mask_map_t k_comp_masks[] = {
#define X(name) { #name, CMP_##name },
    #include "../includes/components.def"
#undef X
};

static const tiled_map_t* g_prefab_spawn_map = NULL;

static char* pf_xstrdup(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char* xml_string_dup_local(struct xml_string* xs) {
    if (!xs) return NULL;
    size_t len = xml_string_length(xs);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    xml_string_copy(xs, (uint8_t*)buf, len);
    buf[len] = '\0';
    return buf;
}

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

static bool comp_from_string(const char* s, ComponentEnum* out_id) {
    if (!s || !out_id) return false;
    for (size_t i = 0; i < sizeof(k_comp_names) / sizeof(k_comp_names[0]); ++i) {
        if (strcasecmp(k_comp_names[i].name, s) == 0) {
            *out_id = k_comp_names[i].id;
            return true;
        }
    }
    return false;
}

static const char* comp_name_from_id(ComponentEnum id) {
    for (size_t i = 0; i < sizeof(k_comp_names) / sizeof(k_comp_names[0]); ++i) {
        if (k_comp_names[i].id == id) {
            return k_comp_names[i].name;
        }
    }
    return NULL;
}

static uint32_t mask_from_string(const char* s, bool* out_ok) {
    if (out_ok) *out_ok = false;
    if (!s) return 0;
    uint32_t mask = 0;
    const char* p = s;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        const char* start = p;
        while (*p && *p != '|' && *p != ',' && !isspace((unsigned char)*p)) p++;
        size_t len = (size_t)(p - start);
        if (len > 0) {
            for (size_t i = 0; i < sizeof(k_comp_masks) / sizeof(k_comp_masks[0]); ++i) {
                if (strncasecmp(k_comp_masks[i].name, start, len) == 0 && k_comp_masks[i].name[len] == '\0') {
                    mask |= k_comp_masks[i].mask;
                    if (out_ok) *out_ok = true;
                    break;
                }
            }
        }
        while (*p && (*p == '|' || *p == ',' || isspace((unsigned char)*p))) p++;
    }
    if (!mask && s && isdigit((unsigned char)*s)) {
        mask = (uint32_t)strtoul(s, NULL, 0);
        if (out_ok) *out_ok = true;
    }
    return mask;
}

static bool parse_float(const char* s, float* out_v) {
    if (!s || !out_v) return false;
    char* end = NULL;
    float v = strtof(s, &end);
    if (end == s) return false;
    *out_v = v;
    return true;
}

static bool parse_int(const char* s, int* out_v) {
    if (!s || !out_v) return false;
    char* end = NULL;
    int v = (int)strtol(s, &end, 0);
    if (end == s) return false;
    *out_v = v;
    return true;
}

static facing_t parse_facing(const char* s, facing_t fallback) {
    if (!s) return fallback;
    if (strcasecmp(s, "n") == 0 || strcasecmp(s, "north") == 0) return DIR_NORTH;
    if (strcasecmp(s, "ne") == 0 || strcasecmp(s, "northeast") == 0) return DIR_NORTHEAST;
    if (strcasecmp(s, "e") == 0 || strcasecmp(s, "east") == 0) return DIR_EAST;
    if (strcasecmp(s, "se") == 0 || strcasecmp(s, "southeast") == 0) return DIR_SOUTHEAST;
    if (strcasecmp(s, "s") == 0 || strcasecmp(s, "south") == 0) return DIR_SOUTH;
    if (strcasecmp(s, "sw") == 0 || strcasecmp(s, "southwest") == 0) return DIR_SOUTHWEST;
    if (strcasecmp(s, "w") == 0 || strcasecmp(s, "west") == 0) return DIR_WEST;
    if (strcasecmp(s, "nw") == 0 || strcasecmp(s, "northwest") == 0) return DIR_NORTHWEST;
    return fallback;
}

static PhysicsType parse_phys_type(const char* s, PhysicsType fallback) {
    if (!s) return fallback;
    if (strcasecmp(s, "dynamic") == 0) return PHYS_DYNAMIC;
    if (strcasecmp(s, "kinematic") == 0) return PHYS_KINEMATIC;
    if (strcasecmp(s, "static") == 0) return PHYS_STATIC;
    if (strcasecmp(s, "none") == 0) return PHYS_NONE;
    if (isdigit((unsigned char)*s)) return (PhysicsType)atoi(s);
    return fallback;
}

static item_kind_t parse_item_kind(const char* s, item_kind_t fallback) {
    if (!s) return fallback;
    if (strcasecmp(s, "coin") == 0) return ITEM_COIN;
    if (strcasecmp(s, "hat") == 0) return ITEM_HAT;
    if (isdigit((unsigned char)*s)) return (item_kind_t)atoi(s);
    return fallback;
}

static billboard_state_t parse_billboard_state(const char* s, billboard_state_t fallback) {
    if (!s) return fallback;
    if (strcasecmp(s, "active") == 0) return BILLBOARD_ACTIVE;
    if (strcasecmp(s, "inactive") == 0) return BILLBOARD_INACTIVE;
    return fallback;
}

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

static bool parse_anim_sequence(struct xml_node* anim_node, prefab_anim_def_t* def) {
    if (!def) return false;
    prefab_anim_seq_t seq = {0};
    seq.name = node_attr_strdup_local(anim_node, "name");

    size_t child_count = xml_node_children(anim_node);
    for (size_t i = 0; i < child_count; ++i) {
        struct xml_node* frame_node = xml_node_child(anim_node, i);
        if (!node_name_is_local(frame_node, "frame")) continue;
        int col = 0, row = 0;
        char* scol = node_attr_strdup_local(frame_node, "col");
        char* srow = node_attr_strdup_local(frame_node, "row");
        if (scol) { col = atoi(scol); free(scol); }
        if (srow) { row = atoi(srow); free(srow); }
        anim_frame_coord_t fc = { (uint8_t)col, (uint8_t)row };
        anim_frame_coord_t* tmp = (anim_frame_coord_t*)realloc(seq.frames, (seq.frame_count + 1) * sizeof(anim_frame_coord_t));
        if (!tmp) {
            free(seq.frames);
            free(seq.name);
            return false;
        }
        seq.frames = tmp;
        seq.frames[seq.frame_count++] = fc;
    }

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

static bool parse_component_node(struct xml_node* comp_node, prefab_component_t* out) {
    memset(out, 0, sizeof(*out));
    char* type = node_attr_strdup_local(comp_node, "type");
    ComponentEnum id;
    if (!type || !comp_from_string(type, &id)) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "prefab: unknown component type");
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

    if (out->id == ENUM_ANIM && !out->anim) {
        if (!parse_anim_component(comp_node, out)) {
            free_component(out);
            return false;
        }
    }

    return true;
}

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
    *c = (prefab_component_t){0};
}

bool prefab_load(const char* path, prefab_t* out_prefab) {
    if (!path || !out_prefab) return false;
    *out_prefab = (prefab_t){0};

    struct xml_document* doc = load_xml_document(path);
    if (!doc) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "prefab: failed to read %s", path);
        return false;
    }
    struct xml_node* root = xml_document_root(doc);
    if (!root || !node_name_is_local(root, "prefab")) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "prefab: root must be <prefab> in %s", path);
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

void prefab_free(prefab_t* prefab) {
    if (!prefab) return;
    free(prefab->name);
    if (prefab->components) {
        for (size_t i = 0; i < prefab->component_count; ++i) {
            free_component(&prefab->components[i]);
        }
    }
    free(prefab->components);
    *prefab = (prefab_t){0};
}

static const char* find_prop(const prefab_component_t* comp, const char* field) {
    if (!comp || !field) return NULL;
    for (size_t i = 0; i < comp->prop_count; ++i) {
        if (comp->props[i].name && strcasecmp(comp->props[i].name, field) == 0) {
            return comp->props[i].value;
        }
    }
    return NULL;
}

static const char* override_value(const prefab_component_t* comp, const tiled_object_t* obj, const char* field) {
    if (!obj || !field) return NULL;
    char key[128];
    if (comp && comp->type_name) {
        snprintf(key, sizeof(key), "%s.%s", comp->type_name, field);
        const char* v = tiled_object_get_property_value(obj, key);
        if (v) return v;
    }
    return tiled_object_get_property_value(obj, field);
}

static const char* combined_value(const prefab_component_t* comp, const tiled_object_t* obj, const char* field) {
    const char* override = override_value(comp, obj, field);
    if (override) return override;
    return find_prop(comp, field);
}

static v2f object_position_default(const tiled_object_t* obj) {
    if (!obj) return v2f_make(0.0f, 0.0f);
    float x = obj->x;
    float y = obj->y;
    bool is_tile_obj = obj->gid != 0;
    if (obj->w > 0.0f || obj->h > 0.0f) {
        x += obj->w * 0.5f;
        y += is_tile_obj ? -obj->h * 0.5f : obj->h * 0.5f;
    }
    return v2f_make(x, y);
}

static bool parse_rect(const char* s, rectf* out) {
    if (!s || !out) return false;
    float x = 0, y = 0, w = 0, h = 0;
    if (sscanf(s, "%f,%f,%f,%f", &x, &y, &w, &h) != 4) return false;
    *out = rectf_xywh(x, y, w, h);
    return true;
}

static void apply_pos_component(const prefab_component_t* comp, const tiled_object_t* obj, ecs_entity_t e, bool* added_pos) {
    float x = 0.0f, y = 0.0f;
    bool have = false;
    const char* sx = combined_value(comp, obj, "x");
    const char* sy = combined_value(comp, obj, "y");
    have = parse_float(sx, &x) && parse_float(sy, &y);
    if (!have) {
        v2f p = object_position_default(obj);
        x = p.x;
        y = p.y;
        const char* ox = override_value(comp, obj, "x");
        const char* oy = override_value(comp, obj, "y");
        if (ox) parse_float(ox, &x);
        if (oy) parse_float(oy, &y);
    }
    cmp_add_position(e, x, y);
    if (added_pos) *added_pos = true;
}

static void apply_vel_component(const prefab_component_t* comp, const tiled_object_t* obj, ecs_entity_t e) {
    float vx = 0.0f, vy = 0.0f;
    parse_float(combined_value(comp, obj, "x"), &vx);
    parse_float(combined_value(comp, obj, "y"), &vy);
    facing_t dir = parse_facing(combined_value(comp, obj, "dir"), DIR_SOUTH);
    cmp_add_velocity(e, vx, vy, dir);
}

static void apply_sprite_component(const prefab_component_t* comp, const tiled_object_t* obj, ecs_entity_t e) {
    const char* path = combined_value(comp, obj, "path");
    if (!path) path = combined_value(comp, obj, "tex");
    rectf src = rectf_xywh(0, 0, 0, 0);
    const char* src_str = combined_value(comp, obj, "src");
    parse_rect(src_str, &src);

    float x = src.x, y = src.y, w = src.w, h = src.h;
    parse_float(combined_value(comp, obj, "src_x"), &x);
    parse_float(combined_value(comp, obj, "src_y"), &y);
    parse_float(combined_value(comp, obj, "src_w"), &w);
    parse_float(combined_value(comp, obj, "src_h"), &h);
    src = rectf_xywh(x, y, w, h);

    float ox = 0.0f, oy = 0.0f;
    parse_float(combined_value(comp, obj, "ox"), &ox);
    parse_float(combined_value(comp, obj, "oy"), &oy);

    if (path) {
        cmp_add_sprite_path(e, path, src, ox, oy);
    } else {
        LOGC(LOGCAT_MAIN, LOG_LVL_WARN, "prefab sprite missing path");
    }
}

static void apply_anim_component(const prefab_component_t* comp, const tiled_object_t* obj, ecs_entity_t e) {
    if (!comp || !comp->anim) return;
    int frame_w = comp->anim->frame_w;
    int frame_h = comp->anim->frame_h;
    float fps = comp->anim->fps;
    parse_int(combined_value(comp, obj, "frame_w"), &frame_w);
    parse_int(combined_value(comp, obj, "frame_h"), &frame_h);
    parse_float(combined_value(comp, obj, "fps"), &fps);

    int anim_count = (int)comp->anim->seq_count;
    if (anim_count > MAX_ANIMS) anim_count = MAX_ANIMS;

    int frames_per_anim[MAX_ANIMS] = {0};
    anim_frame_coord_t frames[MAX_ANIMS][MAX_FRAMES];
    memset(frames, 0, sizeof(frames));
    for (int i = 0; i < anim_count; ++i) {
        const prefab_anim_seq_t* seq = &comp->anim->seqs[i];
        int count = (int)seq->frame_count;
        if (count > MAX_FRAMES) count = MAX_FRAMES;
        frames_per_anim[i] = count;
        for (int f = 0; f < count; ++f) {
            frames[i][f] = seq->frames[f];
        }
    }

    cmp_add_anim(e, frame_w, frame_h, anim_count, frames_per_anim, frames, fps);
}

static void apply_phys_component(const prefab_component_t* comp, const tiled_object_t* obj, ecs_entity_t e) {
    PhysicsType type = parse_phys_type(combined_value(comp, obj, "type"), PHYS_DYNAMIC);
    float mass = 1.0f, restitution = 0.0f, friction = 0.8f;
    parse_float(combined_value(comp, obj, "mass"), &mass);
    parse_float(combined_value(comp, obj, "restitution"), &restitution);
    parse_float(combined_value(comp, obj, "friction"), &friction);
    cmp_add_phys_body(e, type, mass, restitution, friction);
}

static void apply_item_component(const prefab_component_t* comp, const tiled_object_t* obj, ecs_entity_t e) {
    item_kind_t k = parse_item_kind(combined_value(comp, obj, "kind"), ITEM_COIN);
    cmp_add_item(e, k);
}

static void apply_vendor_component(const prefab_component_t* comp, const tiled_object_t* obj, ecs_entity_t e) {
    item_kind_t sells = parse_item_kind(combined_value(comp, obj, "sells"), ITEM_COIN);
    int price = 0;
    parse_int(combined_value(comp, obj, "price"), &price);
    cmp_add_vendor(e, sells, price);
}

static void apply_follow_component(const prefab_component_t* comp, const tiled_object_t* obj, ecs_entity_t e) {
    float dist = 0.0f, speed = 0.0f, vision = -1.0f;
    parse_float(combined_value(comp, obj, "desired_distance"), &dist);
    parse_float(combined_value(comp, obj, "max_speed"), &speed);
    parse_float(combined_value(comp, obj, "vision_range"), &vision);
    ecs_entity_t target = ecs_null();
    const char* target_str = combined_value(comp, obj, "target");
    if (target_str && strcasecmp(target_str, "player") == 0) {
        target = ecs_find_player();
    }
    cmp_add_follow(e, target, dist, speed);
    int idx = ent_index_checked(e);
    if (idx >= 0) {
        cmp_follow[idx].vision_range = vision;
    }
}

static void apply_col_component(const prefab_component_t* comp, const tiled_object_t* obj, ecs_entity_t e) {
    float hx = 0.0f, hy = 0.0f;
    bool have_hx = parse_float(combined_value(comp, obj, "hx"), &hx);
    bool have_hy = parse_float(combined_value(comp, obj, "hy"), &hy);
    if (!have_hx && obj) {
        hx = (obj->w > 0.0f) ? (obj->w * 0.5f) : 0.0f;
    }
    if (!have_hy && obj) {
        hy = (obj->h > 0.0f) ? (obj->h * 0.5f) : 0.0f;
    }
    cmp_add_size(e, hx, hy);
}

static void apply_liftable_component(const prefab_component_t* comp, const tiled_object_t* obj, ecs_entity_t e) {
    cmp_add_liftable(e);
    int idx = ent_index_checked(e);
    if (idx < 0) return;
    float tmp = 0.0f;
    if (parse_float(combined_value(comp, obj, "carry_height"), &tmp)) cmp_liftable[idx].carry_height = tmp;
    if (parse_float(combined_value(comp, obj, "carry_distance"), &tmp)) cmp_liftable[idx].carry_distance = tmp;
    if (parse_float(combined_value(comp, obj, "pickup_distance"), &tmp)) cmp_liftable[idx].pickup_distance = tmp;
    if (parse_float(combined_value(comp, obj, "pickup_radius"), &tmp)) cmp_liftable[idx].pickup_radius = tmp;
    if (parse_float(combined_value(comp, obj, "throw_speed"), &tmp)) cmp_liftable[idx].throw_speed = tmp;
    if (parse_float(combined_value(comp, obj, "throw_vertical_speed"), &tmp)) cmp_liftable[idx].throw_vertical_speed = tmp;
    if (parse_float(combined_value(comp, obj, "gravity"), &tmp)) cmp_liftable[idx].gravity = tmp;
    if (parse_float(combined_value(comp, obj, "air_friction"), &tmp)) cmp_liftable[idx].air_friction = tmp;
    if (parse_float(combined_value(comp, obj, "bounce_damping"), &tmp)) cmp_liftable[idx].bounce_damping = tmp;
}

static void apply_trigger_component(const prefab_component_t* comp, const tiled_object_t* obj, ecs_entity_t e) {
    float pad = 0.0f;
    bool have_pad = parse_float(combined_value(comp, obj, "pad"), &pad);
    if (!have_pad && obj) {
        const char* prox = tiled_object_get_property_value(obj, "proximity_radius");
        if (!prox) prox = combined_value(comp, obj, "proximity_radius");
        parse_float(prox, &pad);
    }
    bool ok = false;
    uint32_t mask = mask_from_string(combined_value(comp, obj, "target_mask"), &ok);
    cmp_add_trigger(e, pad, mask);
}

static void apply_billboard_component(const prefab_component_t* comp, const tiled_object_t* obj, ecs_entity_t e) {
    const char* text = combined_value(comp, obj, "text");
    if (!text) text = "";
    float y_off = 0.0f, linger = 0.0f;
    parse_float(combined_value(comp, obj, "y_offset"), &y_off);
    parse_float(combined_value(comp, obj, "linger"), &linger);
    billboard_state_t state = parse_billboard_state(combined_value(comp, obj, "state"), BILLBOARD_ACTIVE);
    cmp_add_billboard(e, text, y_off, linger, state);
}

typedef DA(door_tile_xy_t) door_tile_xy_da_t;

static int parse_door_tiles(const char* s, door_tile_xy_da_t* out_xy) {
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

static int collect_door_tiles_from_object(const tiled_map_t* map, const tiled_object_t* obj, door_tile_xy_da_t* tiles_xy) {
    if (!map || !obj || !tiles_xy) return 0;
    DA_CLEAR(tiles_xy);
    int tile_count = 0;
    if (obj->door_tile_count > 0) {
        DA_RESERVE(tiles_xy, obj->door_tile_count);
        for (int t = 0; t < obj->door_tile_count; ++t) {
            DA_APPEND(tiles_xy, ((door_tile_xy_t){ obj->door_tiles[t][0], obj->door_tiles[t][1] }));
        }
    } else {
        float ow = (obj->w > 0.0f) ? obj->w : (float)map->tilewidth;
        float oh = (obj->h > 0.0f) ? obj->h : (float)map->tileheight;
        float top = obj->y - oh;
        float left = obj->x;
        int start_tx = (int)floorf(left / map->tilewidth);
        int end_tx   = (int)ceilf((left + ow) / map->tilewidth);
        int start_ty = (int)floorf(top / map->tileheight);
        int end_ty   = (int)ceilf((top + oh) / map->tileheight);
        for (int ty = start_ty; ty < end_ty; ++ty) {
            for (int tx = start_tx; tx < end_tx; ++tx) {
                DA_APPEND(tiles_xy, ((door_tile_xy_t){ tx, ty }));
            }
        }
    }
    tile_count = (int)tiles_xy->size;
    return tile_count;
}

static void resolve_door_tiles(const tiled_map_t* map, cmp_door_t* door) {
    if (!map || !door) return;
    const uint32_t FLIP_H = 0x80000000;
    const uint32_t FLIP_V = 0x40000000;
    const uint32_t FLIP_D = 0x20000000;
    const uint32_t GID_MASK = 0x1FFFFFFF;

    for (size_t t = 0; t < door->tiles.size; ++t) {
        int tx = door->tiles.data[t].x;
        int ty = door->tiles.data[t].y;
        uint32_t raw_gid = 0;
        int found_layer = -1;
        for (size_t li = map->layer_count; li-- > 0; ) {
            const tiled_layer_t *layer = &map->layers[li];
            if (tx < 0 || ty < 0 || tx >= layer->width || ty >= layer->height) continue;
            uint32_t gid = layer->gids[(size_t)ty * (size_t)layer->width + (size_t)tx];
            if (gid == 0) continue;
            raw_gid = gid;
            found_layer = (int)li;
            break;
        }
        door->tiles.data[t].layer_idx = found_layer;
        door->tiles.data[t].flip_flags = raw_gid & (FLIP_H | FLIP_V | FLIP_D);
        uint32_t bare_gid = raw_gid & GID_MASK;
        int chosen_ts = -1;
        int local_id = 0;
        for (size_t si = 0; si < map->tileset_count; ++si) {
            const tiled_tileset_t *ts = &map->tilesets[si];
            int local = (int)bare_gid - ts->first_gid;
            if (local < 0 || local >= ts->tilecount) continue;
            chosen_ts = (int)si;
            local_id = local;
        }
        door->tiles.data[t].tileset_idx = chosen_ts;
        door->tiles.data[t].base_tile_id = local_id;
    }
}

static void apply_door_component(const prefab_component_t* comp, const tiled_object_t* obj, ecs_entity_t e) {
    float prox_r = 64.0f;
    parse_float(combined_value(comp, obj, "proximity_radius"), &prox_r);
    float prox_ox = 0.0f, prox_oy = 0.0f;
    parse_float(combined_value(comp, obj, "proximity_off_x"), &prox_ox);
    parse_float(combined_value(comp, obj, "proximity_off_y"), &prox_oy);

    door_tile_xy_da_t tiles = {0};
    int tile_count = parse_door_tiles(combined_value(comp, obj, "tiles"), &tiles);
    if (tile_count <= 0) {
        tile_count = parse_door_tiles(combined_value(comp, obj, "door_tiles"), &tiles);
    }

    cmp_add_door(e, prox_r, prox_ox, prox_oy, tile_count, tile_count > 0 ? tiles.data : NULL);
    int idx = ent_index_checked(e);
    if (idx >= 0 && g_prefab_spawn_map) {
        if (cmp_door[idx].tiles.size == 0) {
            collect_door_tiles_from_object(g_prefab_spawn_map, obj, &tiles);
            cmp_add_door(e, prox_r, prox_ox, prox_oy, (int)tiles.size, tiles.size > 0 ? tiles.data : NULL);
        }
        resolve_door_tiles(g_prefab_spawn_map, &cmp_door[idx]);
    }
    DA_FREE(&tiles);
}

static void apply_component(const prefab_component_t* comp, const tiled_object_t* obj, ecs_entity_t e, bool* added_pos) {
    switch (comp->id) {
        case ENUM_POS:        apply_pos_component(comp, obj, e, added_pos); break;
        case ENUM_VEL:        apply_vel_component(comp, obj, e); break;
        case ENUM_PHYS_BODY:  apply_phys_component(comp, obj, e); break;
        case ENUM_SPR:        apply_sprite_component(comp, obj, e); break;
        case ENUM_ANIM:       apply_anim_component(comp, obj, e); break;
        case ENUM_PLAYER:     cmp_add_player(e); break;
        case ENUM_ITEM:       apply_item_component(comp, obj, e); break;
        case ENUM_INV:        cmp_add_inventory(e); break;
        case ENUM_VENDOR:     apply_vendor_component(comp, obj, e); break;
        case ENUM_FOLLOW:     apply_follow_component(comp, obj, e); break;
        case ENUM_COL:        apply_col_component(comp, obj, e); break;
        case ENUM_LIFTABLE:   apply_liftable_component(comp, obj, e); break;
        case ENUM_TRIGGER:    apply_trigger_component(comp, obj, e); break;
        case ENUM_BILLBOARD:  apply_billboard_component(comp, obj, e); break;
        case ENUM_DOOR:       apply_door_component(comp, obj, e); break;
        default:
            LOGC(LOGCAT_MAIN, LOG_LVL_WARN, "prefab: component %s not handled", comp_name_from_id(comp->id));
            break;
    }
}

static const char* object_prop_only(const tiled_object_t* obj, const char* comp_name, const char* field) {
    if (!obj || !field || !comp_name) return NULL;
    char key[128];
    snprintf(key, sizeof(key), "%s.%s", comp_name, field);
    const char* v = tiled_object_get_property_value(obj, key);
    if (v) return v;
    return tiled_object_get_property_value(obj, field);
}

ecs_entity_t prefab_spawn_entity(const prefab_t* prefab, const tiled_object_t* obj) {
    ecs_entity_t e = ecs_create();
    bool added_pos = false;
    if (!prefab) return e;

    for (size_t i = 0; i < prefab->component_count; ++i) {
        apply_component(&prefab->components[i], obj, e, &added_pos);
    }

    if (!added_pos) {
        // Default POS from object, still honoring overrides like POS.x
        float x = object_position_default(obj).x;
        float y = object_position_default(obj).y;
        const char* ox = object_prop_only(obj, "POS", "x");
        const char* oy = object_prop_only(obj, "POS", "y");
        parse_float(ox, &x);
        parse_float(oy, &y);
        cmp_add_position(e, x, y);
    }

    return e;
}

ecs_entity_t prefab_spawn_from_path(const char* prefab_path, const tiled_object_t* obj) {
    prefab_t prefab;
    if (!prefab_load(prefab_path, &prefab)) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "prefab: could not load %s", prefab_path ? prefab_path : "(null)");
        return ecs_null();
    }
    ecs_entity_t e = prefab_spawn_entity(&prefab, obj);
    prefab_free(&prefab);
    return e;
}

static char* join_relative_path(const char* base_path, const char* rel) {
    if (!rel || rel[0] == '\0') return NULL;
    if (!base_path || rel[0] == '/' || rel[0] == '\\') return pf_xstrdup(rel);
    const char* slash = strrchr(base_path, '/');
#ifdef _WIN32
    const char* bslash = strrchr(base_path, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    if (!slash) return pf_xstrdup(rel);
    size_t dir_len = (size_t)(slash - base_path) + 1;
    size_t rel_len = strlen(rel);
    char* buf = (char*)malloc(dir_len + rel_len + 1);
    if (!buf) return NULL;
    memcpy(buf, base_path, dir_len);
    memcpy(buf + dir_len, rel, rel_len);
    buf[dir_len + rel_len] = '\0';
    return buf;
}

size_t prefab_spawn_from_map(const tiled_map_t* map, const char* tmx_path) {
    if (!map) return 0;
    const tiled_map_t* prev_map = g_prefab_spawn_map;
    g_prefab_spawn_map = map;
    size_t spawned = 0;
    for (size_t i = 0; i < map->object_count; ++i) {
        const tiled_object_t* obj = &map->objects[i];
        const char* prefab_rel = tiled_object_get_property_value(obj, "entityprefab");
        if (!prefab_rel) continue;

        char* resolved = join_relative_path(tmx_path, prefab_rel);
        const char* path = resolved ? resolved : prefab_rel;
        ecs_entity_t e = prefab_spawn_from_path(path, obj);
        if (resolved) free(resolved);
        int idx = ent_index_checked(e);
        if (idx >= 0) spawned++;
    }
    g_prefab_spawn_map = prev_map;
    return spawned;
}
