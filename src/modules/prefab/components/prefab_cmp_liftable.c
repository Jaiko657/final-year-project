#include "modules/prefab/prefab_cmp.h"

bool prefab_cmp_liftable_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_liftable_t* out_liftable)
{
    if (!out_liftable) return false;

    float tmp = 0.0f;
    *out_liftable = (prefab_cmp_liftable_t){0};

    if (prefab_parse_float(prefab_combined_value(comp, obj, "carry_height"), &tmp)) { out_liftable->has_carry_height = true; out_liftable->carry_height = tmp; }
    if (prefab_parse_float(prefab_combined_value(comp, obj, "carry_distance"), &tmp)) { out_liftable->has_carry_distance = true; out_liftable->carry_distance = tmp; }
    if (prefab_parse_float(prefab_combined_value(comp, obj, "pickup_distance"), &tmp)) { out_liftable->has_pickup_distance = true; out_liftable->pickup_distance = tmp; }
    if (prefab_parse_float(prefab_combined_value(comp, obj, "pickup_radius"), &tmp)) { out_liftable->has_pickup_radius = true; out_liftable->pickup_radius = tmp; }
    if (prefab_parse_float(prefab_combined_value(comp, obj, "throw_speed"), &tmp)) { out_liftable->has_throw_speed = true; out_liftable->throw_speed = tmp; }
    if (prefab_parse_float(prefab_combined_value(comp, obj, "throw_vertical_speed"), &tmp)) { out_liftable->has_throw_vertical_speed = true; out_liftable->throw_vertical_speed = tmp; }
    if (prefab_parse_float(prefab_combined_value(comp, obj, "gravity"), &tmp)) { out_liftable->has_gravity = true; out_liftable->gravity = tmp; }
    if (prefab_parse_float(prefab_combined_value(comp, obj, "air_friction"), &tmp)) { out_liftable->has_air_friction = true; out_liftable->air_friction = tmp; }
    if (prefab_parse_float(prefab_combined_value(comp, obj, "bounce_damping"), &tmp)) { out_liftable->has_bounce_damping = true; out_liftable->bounce_damping = tmp; }

    return true;
}
