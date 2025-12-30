#include "modules/prefab/prefab_cmp.h"

bool prefab_cmp_col_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_col_t* out_col)
{
    if (!out_col) return false;
    float hx = 0.0f, hy = 0.0f;

    bool have_hx = false;
    bool have_hy = false;

    const char* prefab_hx = prefab_find_prop(comp, "hx");
    const char* prefab_hy = prefab_find_prop(comp, "hy");
    if (prefab_parse_float(prefab_hx, &hx)) have_hx = true;
    if (prefab_parse_float(prefab_hy, &hy)) have_hy = true;

    if (!have_hx && obj && prefab_parse_float(prefab_override_value(comp, obj, "hx"), &hx)) have_hx = true;
    if (!have_hy && obj && prefab_parse_float(prefab_override_value(comp, obj, "hy"), &hy)) have_hy = true;
    //override with tiled object width/height if not specified
    if (!have_hx && obj && obj->w > 0.0f) hx = obj->w * 0.5f;
    if (!have_hy && obj && obj->h > 0.0f) hy = obj->h * 0.5f;

    *out_col = (prefab_cmp_col_t){ hx, hy };
    return true;
}
