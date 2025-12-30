#include "modules/prefab/prefab_cmp.h"

bool prefab_cmp_trigger_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_trigger_t* out_trigger)
{
    if (!out_trigger) return false;
    float pad = 0.0f;
    bool have_pad = prefab_parse_float(prefab_combined_value(comp, obj, "pad"), &pad);
    if (!have_pad && obj) {
        const char* prox = tiled_object_get_property_value(obj, "proximity_radius");
        if (!prox) prox = prefab_combined_value(comp, obj, "proximity_radius");
        prefab_parse_float(prox, &pad);
    }

    bool ok = false;
    uint32_t mask = prefab_parse_mask(prefab_combined_value(comp, obj, "target_mask"), &ok);
    *out_trigger = (prefab_cmp_trigger_t){ pad, mask };
    return true;
}
