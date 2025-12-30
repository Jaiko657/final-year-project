#include "modules/prefab/prefab_cmp.h"

bool prefab_cmp_vel_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_vel_t* out_vel)
{
    if (!out_vel) return false;

    float vx = 0.0f, vy = 0.0f;
    prefab_parse_float(prefab_combined_value(comp, obj, "x"), &vx);
    prefab_parse_float(prefab_combined_value(comp, obj, "y"), &vy);
    facing_t dir = prefab_parse_facing(prefab_combined_value(comp, obj, "dir"), DIR_SOUTH);
    *out_vel = (prefab_cmp_vel_t){ vx, vy, dir };
    return true;
}
