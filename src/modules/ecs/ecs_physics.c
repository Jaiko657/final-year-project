#include "modules/ecs/ecs_internal.h"

static bool has_body_requirements(int idx)
{
    const uint32_t req = (CMP_POS | CMP_COL | CMP_PHYS_BODY);
    return ecs_alive_idx(idx) && ((ecs_mask[idx] & req) == req);
}

void ecs_phys_body_create_for_entity(int idx)
{
    if (!has_body_requirements(idx)) return;

    cmp_phys_body_t* pb = &cmp_phys_body[idx];
    if (pb->created) return;

    pb->created = true;
}

void ecs_phys_body_destroy_for_entity(int idx)
{
    if (idx < 0 || idx >= ECS_MAX_ENTITIES) return;
    cmp_phys_body_t* pb = &cmp_phys_body[idx];
    pb->created = false;
}

void ecs_phys_destroy_all(void)
{
    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if (!(ecs_mask[i] & CMP_PHYS_BODY)) continue;
        ecs_phys_body_destroy_for_entity(i);
    }
}
