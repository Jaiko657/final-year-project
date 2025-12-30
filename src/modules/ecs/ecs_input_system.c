#include "modules/ecs/ecs_internal.h"
#include "modules/core/input.h"

static facing_t dir_from_input(const input_t* in, facing_t fallback)
{
    if (in->moveX == 0.0f && in->moveY == 0.0f) {
        return fallback;
    }

    if (in->moveY < 0.0f) {                 // NORTH half
        if      (in->moveX < 0.0f) return DIR_NORTHWEST;
        else if (in->moveX > 0.0f) return DIR_NORTHEAST;
        else                       return DIR_NORTH;
    }
    else if (in->moveY > 0.0f) {           // SOUTH half
        if      (in->moveX < 0.0f) return DIR_SOUTHWEST;
        else if (in->moveX > 0.0f) return DIR_SOUTHEAST;
        else                       return DIR_SOUTH;
    }
    else {                                 // Horizontal only
        if      (in->moveX < 0.0f) return DIR_WEST;
        else if (in->moveX > 0.0f) return DIR_EAST;
    }
    // Shuts up compiler, but not sure if it's even possible to get here.
    return fallback;
}

void sys_input(float dt, const input_t* in)
{
    const float SPEED       = 120.0f;
    const float CHANGE_TIME = 0.04f;   // 40 ms

    for (int e = 0; e < ECS_MAX_ENTITIES; ++e) {
        if (!ecs_alive_idx(e)) continue;
        if ((ecs_mask[e] & (CMP_PLAYER | CMP_VEL)) != (CMP_PLAYER | CMP_VEL)) continue;

        cmp_velocity_t*      v = &cmp_vel[e];
        smoothed_facing_t*   f = &v->facing;

        v->x = in->moveX * SPEED;
        v->y = in->moveY * SPEED;

        const int hasInput = (in->moveX != 0.0f || in->moveY != 0.0f);
        if (!hasInput) {
            f->rawDir        = f->facingDir;
            f->candidateDir  = f->facingDir;
            f->candidateTime = 0.0f;
            break;
        }
        facing_t newRaw = dir_from_input(in, f->facingDir);
        f->rawDir = newRaw;
        if (newRaw == f->facingDir) {
            f->candidateDir  = f->facingDir;
            f->candidateTime = 0.0f;
            break;
        }

        // Add time
        if (newRaw == f->candidateDir) {
            f->candidateTime += dt;
        } else {
            f->candidateDir  = newRaw;
            f->candidateTime = dt;
        }

        // Threshold hit
        if (f->candidateTime >= CHANGE_TIME) {
            f->facingDir     = f->candidateDir;
            f->candidateTime = 0.0f;
        }
    }
}
