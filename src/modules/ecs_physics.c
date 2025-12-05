#include "../includes/ecs_internal.h"
#include "../includes/world_physics.h"
#include <stdint.h>

static bool has_body_requirements(int idx)
{
    const uint32_t req = (CMP_POS | CMP_COL | CMP_PHYS_BODY);
    return ecs_alive_idx(idx) && ((ecs_mask[idx] & req) == req);
}

void ecs_phys_body_create_for_entity(int idx)
{
    if (!has_body_requirements(idx)) return;

    cmp_phys_body_t* pb = &cmp_phys_body[idx];
    if (pb->cp_body || pb->cp_shape) return; // already created

    if (!world_physics_ready()) return;

    cpBody* body = NULL;
    switch (pb->type) {
        case PHYS_DYNAMIC:
            body = cpBodyNew(pb->mass,
                cpMomentForBox(pb->mass, cmp_col[idx].hx * 2.0f, cmp_col[idx].hy * 2.0f));
            cpBodySetMoment(body, INFINITY);      // infinite moment = no rotation
            cpBodySetAngle(body, 0.0f);          // reset angle just in case
            break;
        case PHYS_KINEMATIC:
            body = cpBodyNewKinematic();
            break;
        case PHYS_STATIC:
            body = cpBodyNewStatic();
            break;
        default:
            return;
    }

    cpBodySetPosition(body, cpv(cmp_pos[idx].x, cmp_pos[idx].y));
    cpBodySetUserData(body, (cpDataPointer)(uintptr_t)idx);

    cpShape* shape = cpBoxShapeNew(body, cmp_col[idx].hx * 2.0f, cmp_col[idx].hy * 2.0f, 0.0);
    cpShapeSetFriction(shape, pb->friction);
    cpShapeSetElasticity(shape, pb->restitution);

    if (pb->category_bits || pb->mask_bits) {
        cpShapeFilter f = cpShapeGetFilter(shape);
        if (pb->category_bits) f.categories = pb->category_bits;
        if (pb->mask_bits)     f.mask       = pb->mask_bits;
        cpShapeSetFilter(shape, f);
    }

    if (pb->type != PHYS_STATIC) {
        if (!world_physics_add_body(body)) {
            cpShapeFree(shape);
            cpBodyFree(body);
            return;
        }
    }

    if (!world_physics_add_shape(shape)) {
        if (pb->type != PHYS_STATIC) {
            world_physics_remove_body(body);
            cpBodyFree(body);
        }
        cpShapeFree(shape);
        return;
    }

    pb->cp_body  = body;
    pb->cp_shape = shape;
}

void ecs_phys_body_destroy_for_entity(int idx)
{
    if (idx < 0 || idx >= ECS_MAX_ENTITIES) return;
    cmp_phys_body_t* pb = &cmp_phys_body[idx];
    if (!pb->cp_body && !pb->cp_shape) return;

    if (pb->cp_shape) {
        world_physics_remove_shape(pb->cp_shape);
        cpShapeFree(pb->cp_shape);
        pb->cp_shape = NULL;
    }
    if (pb->cp_body) {
        if (pb->type != PHYS_STATIC) world_physics_remove_body(pb->cp_body);
        cpBodyFree(pb->cp_body);
        pb->cp_body = NULL;
    }
}

void ecs_phys_destroy_all(void)
{
    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if (!(ecs_mask[i] & CMP_PHYS_BODY)) continue;
        ecs_phys_body_destroy_for_entity(i);
    }
}
