#include "../includes/ecs_internal.h"
#include "../includes/logger.h"
#include <string.h>

void cmp_add_anim(
    ecs_entity_t e,
    int frame_w,
    int frame_h,
    int anim_count,
    const int* frames_per_anim,
    const anim_frame_coord_t frames[][MAX_FRAMES],
    float fps)
{
    int i = ent_index_checked(e);
    if (i < 0) return;

    cmp_anim_t* a = &cmp_anim[i];
    memset(a, 0, sizeof(*a));

    a->frame_w       = frame_w;
    a->frame_h       = frame_h;
    a->anim_count    = anim_count;
    a->current_anim  = 0;
    a->frame_index   = 0;
    a->current_time  = 0.0f;
    a->frame_duration = (fps > 0.0f) ? (1.0f / fps) : 0.1f;

    for (int anim = 0; anim < anim_count && anim < MAX_ANIMS; ++anim) {
        int count = frames_per_anim[anim];
        if (count > MAX_FRAMES) count = MAX_FRAMES;

        a->frames_per_anim[anim] = count;
        for (int f = 0; f < count; ++f) {
            a->frames[anim][f] = frames[anim][f];
        }
    }

    ecs_mask[i] |= CMP_ANIM;
}

static void sys_anim_controller_impl(void)
{
    ecs_entity_t player = find_player_handle();
    int idx = ent_index_checked(player);
    if (idx < 0) return;

    if ((ecs_mask[idx] & (CMP_ANIM | CMP_VEL)) != (CMP_ANIM | CMP_VEL))
        return;

    cmp_anim_t*      a = &cmp_anim[idx];
    cmp_velocity_t*  v = &cmp_vel[idx];

    float vx = v->x;
    float vy = v->y;
    float speed2 = vx*vx + vy*vy;

    int dir = (int)v->facing;   // 0..7

    int new_anim;
    if (speed2 < 0.01f * 0.01f) {
        // Idle
        new_anim = 8 + dir;
    } else {
        // Walking
        new_anim = dir;
    }

    if(new_anim >= MAX_ANIMS) {
        LOGC(LOGCAT_ECS, LOG_LVL_ERROR, "New animation: %i outside of max animation %i", new_anim, MAX_ANIMS);
        return;
    }
    if (new_anim != a->current_anim) {
        a->current_anim = new_anim;
        a->frame_index  = 0;
        a->current_time = 0.0f;
    }

    int seq_len = a->frames_per_anim[a->current_anim];
    if (seq_len <= 0) return;
    if (a->frame_index >= seq_len) {
        a->frame_index = 0;
    }
}

static void sys_anim_sprite_impl(float dt)
{
    for (int i = 0; i < ECS_MAX_ENTITIES; ++i)
    {
        if (!ecs_alive_idx(i)) continue;
        if ((ecs_mask[i] & (CMP_SPR | CMP_ANIM)) != (CMP_SPR | CMP_ANIM)) continue;

        cmp_anim_t*   a = &cmp_anim[i];
        cmp_sprite_t* s = &cmp_spr[i];

        int anim = a->current_anim;
        if (anim < 0 || anim >= a->anim_count) continue;

        int seq_len = a->frames_per_anim[anim];
        if (seq_len <= 0) continue;

        // --- advance time and frame index ---
        a->current_time += dt;
        while (a->current_time >= a->frame_duration) {
            a->current_time -= a->frame_duration;
            a->frame_index++;
            if (a->frame_index >= seq_len)
                a->frame_index = 0;
        }

        anim_frame_coord_t fc = a->frames[anim][a->frame_index];
        int frame_w = a->frame_w;
        int frame_h = a->frame_h;
        int x = fc.col * frame_w;
        int y = fc.row * frame_h;

        s->src = rectf_xywh((float)x, (float)y, (float)frame_w, (float)frame_h);
    }
}

// Public adapters (used by ecs_core.c)
void sys_anim_controller_adapt(float dt, const input_t* in)
{
    (void)dt; (void)in;
    sys_anim_controller_impl();
}

void sys_anim_sprite_adapt(float dt, const input_t* in)
{
    (void)in;
    sys_anim_sprite_impl(dt);
}
