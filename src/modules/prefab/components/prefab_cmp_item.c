#include "modules/prefab/prefab_cmp.h"

#include <ctype.h>
#include <stdlib.h>
#include <strings.h>

static item_kind_t parse_item_kind(const char* s, item_kind_t fallback)
{
    if (!s) return fallback;
    if (strcasecmp(s, "coin") == 0) return ITEM_COIN;
    if (strcasecmp(s, "hat") == 0) return ITEM_HAT;
    if (isdigit((unsigned char)*s)) return (item_kind_t)atoi(s);
    return fallback;
}

bool prefab_cmp_item_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_item_t* out_item)
{
    if (!out_item) return false;
    item_kind_t k = parse_item_kind(prefab_combined_value(comp, obj, "kind"), ITEM_COIN);
    *out_item = (prefab_cmp_item_t){ k };
    return true;
}
