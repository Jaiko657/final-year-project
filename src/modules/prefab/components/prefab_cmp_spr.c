#include "modules/prefab/prefab_cmp.h"

#include <stdio.h>

static bool parse_rect(const char* s, rectf* out)
{
    if (!s || !out) return false;
    float x = 0, y = 0, w = 0, h = 0;
    if (sscanf(s, "%f,%f,%f,%f", &x, &y, &w, &h) != 4) return false;
    *out = rectf_xywh(x, y, w, h);
    return true;
}

bool prefab_cmp_spr_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_spr_t* out_spr)
{
    (void)obj;
    if (!out_spr) return false;

    const char* path = prefab_combined_value(comp, obj, "path");
    if (!path) path = prefab_combined_value(comp, obj, "tex");

    rectf src = rectf_xywh(0, 0, 0, 0);
    parse_rect(prefab_combined_value(comp, obj, "src"), &src);

    float x = src.x, y = src.y, w = src.w, h = src.h;
    prefab_parse_float(prefab_combined_value(comp, obj, "src_x"), &x);
    prefab_parse_float(prefab_combined_value(comp, obj, "src_y"), &y);
    prefab_parse_float(prefab_combined_value(comp, obj, "src_w"), &w);
    prefab_parse_float(prefab_combined_value(comp, obj, "src_h"), &h);
    src = rectf_xywh(x, y, w, h);

    float ox = 0.0f, oy = 0.0f;
    prefab_parse_float(prefab_combined_value(comp, obj, "ox"), &ox);
    prefab_parse_float(prefab_combined_value(comp, obj, "oy"), &oy);

    if (!path) return false;

    *out_spr = (prefab_cmp_spr_t){
        .path = path,
        .src = src,
        .ox = ox,
        .oy = oy,
    };
    return true;
}
