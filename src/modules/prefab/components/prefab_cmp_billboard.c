#include "modules/prefab/prefab_cmp.h"

#include <strings.h>

static billboard_state_t parse_billboard_state(const char* s, billboard_state_t fallback)
{
    if (!s) return fallback;
    if (strcasecmp(s, "active") == 0) return BILLBOARD_ACTIVE;
    if (strcasecmp(s, "inactive") == 0) return BILLBOARD_INACTIVE;
    return fallback;
}

bool prefab_cmp_billboard_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_billboard_t* out_billboard)
{
    if (!out_billboard) return false;
    const char* text = prefab_combined_value(comp, obj, "text");
    if (!text) text = "";

    float y_off = 0.0f, linger = 0.0f;
    prefab_parse_float(prefab_combined_value(comp, obj, "y_offset"), &y_off);
    prefab_parse_float(prefab_combined_value(comp, obj, "linger"), &linger);
    billboard_state_t state = parse_billboard_state(prefab_combined_value(comp, obj, "state"), BILLBOARD_ACTIVE);
    *out_billboard = (prefab_cmp_billboard_t){
        .text = text,
        .y_offset = y_off,
        .linger = linger,
        .state = state,
    };
    return true;
}
