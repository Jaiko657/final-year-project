#include "modules/prefab/prefab_cmp.h"

bool prefab_cmp_grav_gun_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_grav_gun_t* out_grav_gun)
{
    if (!out_grav_gun) return false;

    float tmp = 0.0f;
    *out_grav_gun = (prefab_cmp_grav_gun_t){0};

    if (prefab_parse_float(prefab_combined_value(comp, obj, "pickup_distance"), &tmp)) { out_grav_gun->has_pickup_distance = true; out_grav_gun->pickup_distance = tmp; }
    if (prefab_parse_float(prefab_combined_value(comp, obj, "pickup_radius"), &tmp)) { out_grav_gun->has_pickup_radius = true; out_grav_gun->pickup_radius = tmp; }
    if (prefab_parse_float(prefab_combined_value(comp, obj, "max_hold_distance"), &tmp)) { out_grav_gun->has_max_hold_distance = true; out_grav_gun->max_hold_distance = tmp; }
    if (prefab_parse_float(prefab_combined_value(comp, obj, "breakoff_distance"), &tmp)) { out_grav_gun->has_breakoff_distance = true; out_grav_gun->breakoff_distance = tmp; }
    if (prefab_parse_float(prefab_combined_value(comp, obj, "follow_gain"), &tmp)) { out_grav_gun->has_follow_gain = true; out_grav_gun->follow_gain = tmp; }
    if (prefab_parse_float(prefab_combined_value(comp, obj, "max_speed"), &tmp)) { out_grav_gun->has_max_speed = true; out_grav_gun->max_speed = tmp; }
    if (prefab_parse_float(prefab_combined_value(comp, obj, "damping"), &tmp)) { out_grav_gun->has_damping = true; out_grav_gun->damping = tmp; }

    return true;
}
