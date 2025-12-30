#include "modules/prefab/prefab_cmp.h"

bool prefab_cmp_pos_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_pos_t* out_pos)
{
    if (!out_pos) return false;
    float x = 0.0f, y = 0.0f;
    const char* sx = prefab_combined_value(comp, obj, "x");
    const char* sy = prefab_combined_value(comp, obj, "y");
    bool have = prefab_parse_float(sx, &x) && prefab_parse_float(sy, &y);
    if (!have) {
        v2f p = prefab_object_position_default(obj);
        x = p.x;
        y = p.y;
        const char* ox = prefab_override_value(comp, obj, "x");
        const char* oy = prefab_override_value(comp, obj, "y");
        if (ox) prefab_parse_float(ox, &x);
        if (oy) prefab_parse_float(oy, &y);
    }
    *out_pos = (prefab_cmp_pos_t){ x, y };
    return true;
}
