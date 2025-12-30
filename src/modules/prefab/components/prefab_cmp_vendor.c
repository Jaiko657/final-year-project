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

bool prefab_cmp_vendor_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_vendor_t* out_vendor)
{
    if (!out_vendor) return false;
    item_kind_t sells = parse_item_kind(prefab_combined_value(comp, obj, "sells"), ITEM_COIN);
    int price = 0;
    prefab_parse_int(prefab_combined_value(comp, obj, "price"), &price);
    *out_vendor = (prefab_cmp_vendor_t){ sells, price };
    return true;
}
