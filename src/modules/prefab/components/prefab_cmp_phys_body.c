#include "modules/prefab/prefab_cmp.h"

#include <ctype.h>
#include <stdlib.h>
#include <strings.h>

static PhysicsType parse_phys_type(const char* s, PhysicsType fallback)
{
    if (!s) return fallback;
    if (strcasecmp(s, "dynamic") == 0) return PHYS_DYNAMIC;
    if (strcasecmp(s, "kinematic") == 0) return PHYS_KINEMATIC;
    if (strcasecmp(s, "static") == 0) return PHYS_STATIC;
    if (strcasecmp(s, "none") == 0) return PHYS_NONE;
    if (isdigit((unsigned char)*s)) return (PhysicsType)atoi(s);
    return fallback;
}

bool prefab_cmp_phys_body_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_phys_body_t* out_body)
{
    if (!out_body) return false;
    PhysicsType type = parse_phys_type(prefab_combined_value(comp, obj, "type"), PHYS_DYNAMIC);
    float mass = 1.0f;
    prefab_parse_float(prefab_combined_value(comp, obj, "mass"), &mass);
    *out_body = (prefab_cmp_phys_body_t){ type, mass };
    return true;
}
