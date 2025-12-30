#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs_physics.h"
#include "modules/world/world.h"
#include <math.h>

static void resolve_tile_penetration(int i)
{
    const uint32_t req = (CMP_POS | CMP_COL | CMP_PHYS_BODY);
    if ((ecs_mask[i] & req) != req) return;
    if (!cmp_phys_body[i].created) return;
    if ((ecs_mask[i] & CMP_LIFTABLE) && cmp_liftable[i].state != LIFTABLE_STATE_ONGROUND) return;

    float hx = cmp_col[i].hx;
    float hy = cmp_col[i].hy;
    if (hx <= 0.0f || hy <= 0.0f) return;

    float cx = cmp_pos[i].x;
    float cy = cmp_pos[i].y;
    // Resolve after entity/entity overlap so tile response doesn't push sideways.
    if (world_resolve_rect_mtv_px(&cx, &cy, hx, hy)) {
        cmp_pos[i].x = cx;
        cmp_pos[i].y = cy;
    }
}

void sys_physics_integrate_impl(float dt)
{
    if (!world_has_map()) return;

    // Ensure any newly-tagged entities participate in the physics-lite step.
    for (int e = 0; e < ECS_MAX_ENTITIES; ++e) {
        if (!ecs_alive_idx(e)) continue;
        const uint32_t req = (CMP_POS | CMP_COL | CMP_PHYS_BODY);
        if ((ecs_mask[e] & req) != req) continue;
        if (!cmp_phys_body[e].created) {
            ecs_phys_body_create_for_entity(e);
        }
    }

    bool  has_intent[ECS_MAX_ENTITIES];
    for (int e = 0; e < ECS_MAX_ENTITIES; ++e) {
        has_intent[e] = false;
    }

    // Apply intent velocities to positions (physics-lite).
    for (int e = 0; e < ECS_MAX_ENTITIES; ++e) {
        if (!ecs_alive_idx(e)) continue;
        const uint32_t req = (CMP_VEL | CMP_PHYS_BODY);
        if ((ecs_mask[e] & req) != req) continue;

        cmp_velocity_t*  v  = &cmp_vel[e];
        cmp_phys_body_t* pb = &cmp_phys_body[e];
        if (!pb->created) continue;
        if ((ecs_mask[e] & CMP_LIFTABLE) && cmp_liftable[e].state != LIFTABLE_STATE_ONGROUND) {
            // Liftables handle their own airborne motion and disable collisions while carried/thrown.
            v->x = 0.0f;
            v->y = 0.0f;
            continue;
        }

        switch (pb->type) {
            case PHYS_DYNAMIC:
            case PHYS_KINEMATIC:
                if (v->x != 0.0f || v->y != 0.0f) {
                    has_intent[e] = true;
                }
                {
                    float dx = v->x * dt;
                    float dy = v->y * dt;

                    const uint32_t tile_req = (CMP_POS | CMP_COL | CMP_PHYS_BODY);
                    const bool can_collide_tiles = ((ecs_mask[e] & tile_req) == tile_req) && pb->created;

                    if (dx != 0.0f) {
                        float cx = cmp_pos[e].x + dx;
                        float cy = cmp_pos[e].y;
                        if (can_collide_tiles) {
                            world_resolve_rect_axis_px(&cx, &cy, cmp_col[e].hx, cmp_col[e].hy, true);
                        }
                        cmp_pos[e].x = cx;
                        cmp_pos[e].y = cy;
                    }

                    if (dy != 0.0f) {
                        float cx = cmp_pos[e].x;
                        float cy = cmp_pos[e].y + dy;
                        if (can_collide_tiles) {
                            world_resolve_rect_axis_px(&cx, &cy, cmp_col[e].hx, cmp_col[e].hy, false);
                        }
                        cmp_pos[e].x = cx;
                        cmp_pos[e].y = cy;
                    }
                }
                break;
            default:
                break;
        }

        v->x = 0.0f;
        v->y = 0.0f;
    }

    for (int iter = 0; iter < 4; ++iter) {
        for (int a = 0; a < ECS_MAX_ENTITIES; ++a) {
            if (!ecs_alive_idx(a)) continue;
            const uint32_t reqA = (CMP_POS | CMP_COL | CMP_PHYS_BODY);
            if ((ecs_mask[a] & reqA) != reqA) continue;
            if (!cmp_phys_body[a].created) continue;
            if ((ecs_mask[a] & CMP_LIFTABLE) && cmp_liftable[a].state != LIFTABLE_STATE_ONGROUND) continue;

            for (int b = a + 1; b < ECS_MAX_ENTITIES; ++b) {
                if (!ecs_alive_idx(b)) continue;
                const uint32_t reqB = (CMP_POS | CMP_COL | CMP_PHYS_BODY);
                if ((ecs_mask[b] & reqB) != reqB) continue;
                if (!cmp_phys_body[b].created) continue;
                if ((ecs_mask[b] & CMP_LIFTABLE) && cmp_liftable[b].state != LIFTABLE_STATE_ONGROUND) continue;

                const cmp_phys_body_t* pa = &cmp_phys_body[a];
                const cmp_phys_body_t* pb = &cmp_phys_body[b];

                // Optional collision filtering (only if configured on either body).
                if (pa->category_bits || pa->mask_bits || pb->category_bits || pb->mask_bits) {
                    const unsigned int catA = pa->category_bits ? pa->category_bits : 0xFFFFFFFFu;
                    const unsigned int mskA = pa->mask_bits ? pa->mask_bits : 0xFFFFFFFFu;
                    const unsigned int catB = pb->category_bits ? pb->category_bits : 0xFFFFFFFFu;
                    const unsigned int mskB = pb->mask_bits ? pb->mask_bits : 0xFFFFFFFFu;
                    if (((catA & mskB) == 0u) || ((catB & mskA) == 0u)) continue;
                }

                if (pa->type == PHYS_STATIC && pb->type == PHYS_STATIC) continue;

                const float ax = cmp_pos[a].x;
                const float ay = cmp_pos[a].y;
                const float bx = cmp_pos[b].x;
                const float by = cmp_pos[b].y;
                const float ahx = cmp_col[a].hx;
                const float ahy = cmp_col[a].hy;
                const float bhx = cmp_col[b].hx;
                const float bhy = cmp_col[b].hy;

                const float dx = bx - ax;
                const float dy = by - ay;
                const float px = (ahx + bhx) - fabsf(dx);
                const float py = (ahy + bhy) - fabsf(dy);
                if (px <= 0.0f || py <= 0.0f) continue;

                const bool resolve_x = (px < py);
                const float overlap = resolve_x ? px : py;
                const float sign = resolve_x ? (dx >= 0.0f ? 1.0f : -1.0f) : (dy >= 0.0f ? 1.0f : -1.0f);

                float wA = pa->inv_mass;
                float wB = pb->inv_mass;
                if (has_intent[a]) wA *= 2.0f;
                if (has_intent[b]) wB *= 2.0f;

                if (pa->type == PHYS_STATIC) wA = 0.0f;
                if (pb->type == PHYS_STATIC) wB = 0.0f;

                float sum = wA + wB;
                float a_amt = 0.0f;
                float b_amt = 0.0f;
                if (sum > 0.0f) {
                    a_amt = overlap * (wA / sum);
                    b_amt = overlap * (wB / sum);
                } else {
                    // Fallback: split evenly.
                    a_amt = overlap * 0.5f;
                    b_amt = overlap * 0.5f;
                }

                // Separate along chosen axis.
                if (resolve_x) {
                    cmp_pos[a].x -= sign * a_amt;
                    cmp_pos[b].x += sign * b_amt;
                } else {
                    cmp_pos[a].y -= sign * a_amt;
                    cmp_pos[b].y += sign * b_amt;
                }
            }
        }

        for (int e = 0; e < ECS_MAX_ENTITIES; ++e) {
            if (!ecs_alive_idx(e)) continue;
            resolve_tile_penetration(e);
        }
    }
}
