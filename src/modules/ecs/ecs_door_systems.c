#include "modules/ecs/ecs_door_systems.h"

#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs_proximity.h"
#include "modules/core/input.h"
#include "modules/systems/systems.h"
#include "modules/world/world.h"
#include "modules/world/world_door.h"

static void sys_doors_tick(float dt);

static void sys_doors_tick_adapt(float dt, const input_t* in)
{
    (void)dt; (void)in;
    sys_doors_tick(dt);
}

static void sys_doors_tick(float dt)
{
    if (!world_has_map()) return;

    // Build intent from proximity stay/enter
    bool door_should_open[ECS_MAX_ENTITIES] = {0};
    ecs_prox_iter_t stay_it = ecs_prox_stay_begin();
    ecs_prox_view_t v;
    while (ecs_prox_stay_next(&stay_it, &v)) {
        int a = ent_index_checked(v.trigger_owner);
        if (a >= 0 && (ecs_mask[a] & CMP_DOOR)) {
            door_should_open[a] = true;
        }
    }
    ecs_prox_iter_t enter_it = ecs_prox_enter_begin();
    while (ecs_prox_enter_next(&enter_it, &v)) {
        int a = ent_index_checked(v.trigger_owner);
        if (a >= 0 && (ecs_mask[a] & CMP_DOOR)) {
            door_should_open[a] = true;
        }
    }

    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & CMP_DOOR) != CMP_DOOR) continue;
        cmp_door_t *d = &cmp_door[i];
        d->intent_open = door_should_open[i];
    }

    for (int i = 0; i < ECS_MAX_ENTITIES; ++i) {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & CMP_DOOR) != CMP_DOOR) continue;
        cmp_door_t *d = &cmp_door[i];

        int primary_total = world_door_primary_animation_duration(d->world_handle);
        float prev_t = d->anim_time_ms;
        float dt_ms = dt * 1000.0f;
        if (d->intent_open) {
            if (d->state == DOOR_CLOSED) {
                d->anim_time_ms = 0.0f;
                d->state = DOOR_OPENING;
            } else if (d->state == DOOR_CLOSING) {
                if (primary_total > 0) {
                    float clamped = prev_t;
                    if (clamped > (float)primary_total) clamped = (float)primary_total;
                    d->anim_time_ms = (float)primary_total - clamped;
                } else {
                    d->anim_time_ms = 0.0f;
                }
                d->state = DOOR_OPENING;
            }
        } else {
            if (d->state == DOOR_OPEN) {
                d->anim_time_ms = 0.0f;
                d->state = DOOR_CLOSING;
            } else if (d->state == DOOR_OPENING) {
                if (primary_total > 0) {
                    float clamped = prev_t;
                    if (clamped > (float)primary_total) clamped = (float)primary_total;
                    d->anim_time_ms = (float)primary_total - clamped;
                } else {
                    d->anim_time_ms = 0.0f;
                }
                d->state = DOOR_CLOSING;
            }
        }

        if (d->state == DOOR_OPENING || d->state == DOOR_CLOSING) {
            d->anim_time_ms += dt_ms;
        }

        float t_ms = d->anim_time_ms;
        bool play_forward = true;
        switch (d->state) {
            case DOOR_OPENING:
                play_forward = true;
                break;
            case DOOR_OPEN:
                play_forward = true;
                if (primary_total > 0) t_ms = (float)primary_total;
                break;
            case DOOR_CLOSING:
                play_forward = false;
                break;
            case DOOR_CLOSED:
            default:
                play_forward = true;
                t_ms = 0.0f;
                break;
        }
        if (primary_total > 0 && t_ms > (float)primary_total) {
            t_ms = (float)primary_total;
        }

        world_door_apply_state(d->world_handle, t_ms, play_forward);

        if (d->state == DOOR_OPENING) {
            if (primary_total == 0 || d->anim_time_ms >= (float)primary_total) {
                d->state = DOOR_OPEN;
                d->anim_time_ms = (float)primary_total;
            }
        } else if (d->state == DOOR_CLOSING) {
            if (primary_total == 0 || d->anim_time_ms >= (float)primary_total) {
                d->state = DOOR_CLOSED;
                d->anim_time_ms = 0.0f;
            }
        }
    }
}

void ecs_register_door_systems(void)
{
    // Doors: intent, animation, and collision updates all happen on the fixed tick.
    systems_register(PHASE_SIM_POST, 400, sys_doors_tick_adapt, "doors_tick");
}
