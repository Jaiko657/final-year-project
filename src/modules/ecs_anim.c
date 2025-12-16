#include "../includes/ecs_internal.h"
#include "../includes/logger.h"
#include "../includes/bump_alloc.h"
#include <string.h>

// Animation data is flattened into a bump allocator so the per-frame systems
// touch a single contiguous memory region.
static bump_alloc_t g_anim_arena;
static const size_t ANIM_ARENA_BYTES = 128 * 1024;

void ecs_anim_reset_allocator(void)
{
    if (!g_anim_arena.data) {
        bump_init(&g_anim_arena, ANIM_ARENA_BYTES);
    }
    bump_reset(&g_anim_arena);
}

void ecs_anim_shutdown_allocator(void)
{
    bump_free(&g_anim_arena);
}

void cmp_add_anim(
    ecs_entity_t e,
    int frame_w,
    int frame_h,
    int anim_count,
    const int* frames_per_anim,
    const anim_frame_coord_t frames[][MAX_FRAMES],
    float fps)
{
    if (!g_anim_arena.data && !bump_init(&g_anim_arena, ANIM_ARENA_BYTES)) {
        LOGC(LOGCAT_ECS, LOG_LVL_ERROR, "anim: failed to init arena");
        return;
    }

    int i = ent_index_checked(e);
    if (i < 0) return;

    cmp_anim_t* a = &cmp_anim[i];
    memset(a, 0, sizeof(*a));

    int use_anims = anim_count;
    if (use_anims > MAX_ANIMS) use_anims = MAX_ANIMS;

    int total_frames = 0;
    for (int anim = 0; anim < use_anims; ++anim) {
        int count = frames_per_anim ? frames_per_anim[anim] : 0;
        if (count > MAX_FRAMES) count = MAX_FRAMES;
        if (count < 0) count = 0;
        total_frames += count;
    }

    int* fp = bump_alloc_type(&g_anim_arena, int, (size_t)use_anims);
    int* offs = bump_alloc_type(&g_anim_arena, int, (size_t)use_anims);
    anim_frame_coord_t* flat = bump_alloc_type(&g_anim_arena, anim_frame_coord_t, (size_t)total_frames);
    if (!fp || !offs || (total_frames > 0 && !flat)) {
        LOGC(LOGCAT_ECS, LOG_LVL_ERROR, "anim: arena out of space for %d anims (%d frames)", use_anims, total_frames);
        return;
    }

    int acc = 0;
    for (int anim = 0; anim < use_anims; ++anim) {
        int count = frames_per_anim ? frames_per_anim[anim] : 0;
        if (count > MAX_FRAMES) count = MAX_FRAMES;
        if (count < 0) count = 0;

        fp[anim] = count;
        offs[anim] = acc;
        if (count > 0) {
            memcpy(flat + acc, frames[anim], (size_t)count * sizeof(anim_frame_coord_t));
            acc += count;
        }
    }

    a->frame_w       = frame_w;
    a->frame_h       = frame_h;
    a->anim_count    = use_anims;
    a->frames_per_anim = fp;
    a->anim_offsets    = offs;
    a->frames          = flat;
    a->current_anim  = 0;
    a->frame_index   = 0;
    a->current_time  = 0.0f;
    a->frame_duration = (fps > 0.0f) ? (1.0f / fps) : 0.1f;

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

    int dir = (int)v->facing.facingDir;   // 0..7

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

    if (!a->frames_per_anim || !a->anim_offsets || !a->frames) return;

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

        if (!a->frames_per_anim || !a->anim_offsets || !a->frames) continue;

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

        int offset = a->anim_offsets[anim];
        anim_frame_coord_t fc = a->frames[offset + a->frame_index];
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
