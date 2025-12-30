#include "modules/prefab/prefab_cmp.h"

#include <ctype.h>
#include <stdlib.h>
#include <strings.h>

bool prefab_cmp_follow_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_follow_t* out_follow)
{
    if (!out_follow) return false;

    float dist = 0.0f, speed = 0.0f, vision = -1.0f;
    prefab_parse_float(prefab_combined_value(comp, obj, "desired_distance"), &dist);
    prefab_parse_float(prefab_combined_value(comp, obj, "max_speed"), &speed);
    bool have_vision = prefab_parse_float(prefab_combined_value(comp, obj, "vision_range"), &vision);

    prefab_follow_target_kind_t kind = PREFAB_FOLLOW_TARGET_NONE;
    uint32_t target_idx = 0;
    const char* target_str = prefab_combined_value(comp, obj, "target");
    if (target_str && strcasecmp(target_str, "player") == 0) {
        kind = PREFAB_FOLLOW_TARGET_PLAYER;
    } else if (target_str && isdigit((unsigned char)*target_str)) {
        target_idx = (uint32_t)strtoul(target_str, NULL, 10);
        kind = PREFAB_FOLLOW_TARGET_ENTITY_IDX;
    }

    *out_follow = (prefab_cmp_follow_t){
        .target_kind = kind,
        .target_idx = target_idx,
        .desired_distance = dist,
        .max_speed = speed,
        .have_vision_range = have_vision,
        .vision_range = vision,
    };
    return true;
}
