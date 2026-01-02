#include "modules/ecs/ecs_internal.h"
#include "modules/core/input.h"
#include "modules/core/logger.h"
#include "modules/asset/bump_alloc.h"
#include "modules/systems/systems_registration.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// Animation data is flattened into a bump allocator so the per-frame systems
// touch a single contiguous memory region.
#ifndef ECS_ANIM_ARENA_BYTES
#define ECS_ANIM_ARENA_BYTES (128 * 1024)
#endif
static bump_alloc_t g_anim_arena;
static const size_t ANIM_ARENA_BYTES = ECS_ANIM_ARENA_BYTES;

typedef struct {
    uint64_t hash;
    int frame_w;
    int frame_h;
    int anim_count;
    uint32_t fps_bits;
    int total_frames;
    const int* frames_per_anim;
    const int* anim_offsets;
    const anim_frame_coord_t* frames;
} anim_def_entry_t;

static anim_def_entry_t* g_anim_defs = NULL;
static size_t g_anim_defs_count = 0;
static size_t g_anim_defs_cap = 0;

static uint64_t fnv1a64_update(uint64_t h, const void* data, size_t len)
{
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < len; ++i) {
        h ^= (uint64_t)p[i];
        h *= 1099511628211ull;
    }
    return h;
}

static uint64_t anim_def_hash(
    int frame_w,
    int frame_h,
    int anim_count,
    uint32_t fps_bits,
    const int* frames_per_anim,
    const anim_frame_coord_t* frames,
    int frame_buffer_width)
{
    uint64_t h = 1469598103934665603ull;
    h = fnv1a64_update(h, &frame_w, sizeof(frame_w));
    h = fnv1a64_update(h, &frame_h, sizeof(frame_h));
    h = fnv1a64_update(h, &anim_count, sizeof(anim_count));
    h = fnv1a64_update(h, &fps_bits, sizeof(fps_bits));

    for (int anim = 0; anim < anim_count; ++anim) {
        int count = frames_per_anim ? frames_per_anim[anim] : 0;
        if (count < 0) count = 0;
        if (frame_buffer_width > 0 && count > frame_buffer_width) count = frame_buffer_width;

        h = fnv1a64_update(h, &count, sizeof(count));
        if (count > 0) {
            const anim_frame_coord_t* row = NULL;
            if (frames && frame_buffer_width > 0) row = frames + (size_t)anim * frame_buffer_width;
            if (row) {
                h = fnv1a64_update(h, row, (size_t)count * sizeof(anim_frame_coord_t));
            }
        }
    }
    return h;
}

// Compare an animation data against a cached entry
static bool anim_def_equals(
    const anim_def_entry_t* e,
    int frame_w,
    int frame_h,
    int anim_count,
    uint32_t fps_bits,
    int total_frames,
    const int* frames_per_anim,
    const anim_frame_coord_t* frames,
    int frame_buffer_width)
{
    if (!e) return false;
    if (e->frame_w != frame_w || e->frame_h != frame_h) return false;
    if (e->anim_count != anim_count) return false;
    if (e->fps_bits != fps_bits) return false;
    if (e->total_frames != total_frames) return false;
    if (!e->frames_per_anim || !e->anim_offsets || (total_frames > 0 && !e->frames)) return false;

    for (int anim = 0; anim < anim_count; ++anim) {
        int count = frames_per_anim ? frames_per_anim[anim] : 0;
        if (count < 0) count = 0;
        if (frame_buffer_width > 0 && count > frame_buffer_width) count = frame_buffer_width;
        if (e->frames_per_anim[anim] != count) return false;

        if (count > 0) {
            int off = e->anim_offsets[anim];
            const anim_frame_coord_t* row = NULL;
            if (frames && frame_buffer_width > 0) row = frames + (size_t)anim * frame_buffer_width;
            if (!row) return false;
            if (memcmp(e->frames + off, row, (size_t)count * sizeof(anim_frame_coord_t)) != 0) {
                return false;
            }
        }
    }
    return true;
}

void ecs_anim_reset_allocator(void)
{
    if (!g_anim_arena.data) {
        bump_init(&g_anim_arena, ANIM_ARENA_BYTES);
    }
    bump_reset(&g_anim_arena);
    // Any cached pointers into the arena are invalid after reset.
    g_anim_defs_count = 0;
}

void ecs_anim_shutdown_allocator(void)
{
    bump_free(&g_anim_arena);
    free(g_anim_defs);
    g_anim_defs = NULL;
    g_anim_defs_count = 0;
    g_anim_defs_cap = 0;
}

void cmp_add_anim(
    ecs_entity_t e,
    int frame_w,
    int frame_h,
    int anim_count,
    const int* frames_per_anim,
    const anim_frame_coord_t* frames,
    int frame_buffer_width,
    float fps)
{
    if (!g_anim_arena.data) {
        if (!bump_init(&g_anim_arena, ANIM_ARENA_BYTES)) {
            LOGC(LOGCAT_ECS, LOG_LVL_ERROR, "anim: failed to init arena");
            return;
        }
    }

    int i = ent_index_checked(e);
    if (i < 0) return;

    cmp_anim_t* a = &cmp_anim[i];
    memset(a, 0, sizeof(*a));

    int use_anims = anim_count;
    if (use_anims > MAX_ANIMS) use_anims = MAX_ANIMS;
    if (use_anims < 0) use_anims = 0;

    int total_frames = 0;
    for (int anim = 0; anim < use_anims; ++anim) {
        int count = frames_per_anim ? frames_per_anim[anim] : 0;
        if (count < 0) count = 0;
        if (frame_buffer_width > 0 && count > frame_buffer_width) count = frame_buffer_width;
        total_frames += count;
    }

    // Deduplicate animation definitions so multiple spawns of the same prefab
    // don't burn arena space (frame stepping uses contiguous buffers either way).
    uint32_t fps_bits = 0;
    memcpy(&fps_bits, &fps, sizeof(fps_bits));
    const uint64_t h = anim_def_hash(frame_w, frame_h, use_anims, fps_bits, frames_per_anim, frames, frame_buffer_width);
    for (size_t di = 0; di < g_anim_defs_count; ++di) {
        const anim_def_entry_t* def = &g_anim_defs[di];
        if (def->hash != h) continue;
        if (!anim_def_equals(def, frame_w, frame_h, use_anims, fps_bits, total_frames, frames_per_anim, frames, frame_buffer_width)) continue;

        a->frame_w         = frame_w;
        a->frame_h         = frame_h;
        a->anim_count      = use_anims;
        a->frames_per_anim = def->frames_per_anim;
        a->anim_offsets    = def->anim_offsets;
        a->frames          = def->frames;
        a->current_anim    = 0;
        a->frame_index     = 0;
        a->current_time    = 0.0f;
        a->frame_duration  = (fps > 0.0f) ? (1.0f / fps) : 0.1f;
        ecs_mask[i] |= CMP_ANIM;
        return;
    }

    int* fp = bump_alloc_type(&g_anim_arena, int, (size_t)use_anims);
    int* offs = bump_alloc_type(&g_anim_arena, int, (size_t)use_anims);
    anim_frame_coord_t* flat = bump_alloc_type(&g_anim_arena, anim_frame_coord_t, (size_t)total_frames);
    if (!fp || !offs || (total_frames > 0 && !flat)) {
        LOGC(LOGCAT_ECS, LOG_LVL_ERROR,
             "anim: arena out of space for %d anims (%d frames), capacity=%zu bytes",
             use_anims,
             total_frames,
             ANIM_ARENA_BYTES);
        return;
    }

    int acc = 0;
    for (int anim = 0; anim < use_anims; ++anim) {
        int count = frames_per_anim ? frames_per_anim[anim] : 0;
        if (count < 0) count = 0;
        if (frame_buffer_width > 0 && count > frame_buffer_width) count = frame_buffer_width;

        fp[anim] = count;
        offs[anim] = acc;
        if (count > 0) {
            if (frames && frame_buffer_width > 0) {
                const anim_frame_coord_t* row = frames + (size_t)anim * frame_buffer_width;
                memcpy(flat + acc, row, (size_t)count * sizeof(anim_frame_coord_t));
            }
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

    // Cache definition for future reuse (best-effort).
    if (g_anim_defs_count == g_anim_defs_cap) {
        size_t new_cap = g_anim_defs_cap ? (g_anim_defs_cap * 2) : 32;
        anim_def_entry_t* tmp = (anim_def_entry_t*)realloc(g_anim_defs, new_cap * sizeof(*g_anim_defs));
        if (tmp) {
            g_anim_defs = tmp;
            g_anim_defs_cap = new_cap;
        }
    }
    if (g_anim_defs_count < g_anim_defs_cap) {
        g_anim_defs[g_anim_defs_count++] = (anim_def_entry_t){
            .hash = h,
            .frame_w = frame_w,
            .frame_h = frame_h,
            .anim_count = use_anims,
            .fps_bits = fps_bits,
            .total_frames = total_frames,
            .frames_per_anim = fp,
            .anim_offsets = offs,
            .frames = flat,
        };
    }
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
SYSTEMS_ADAPT_VOID(sys_anim_controller_adapt, sys_anim_controller_impl)
SYSTEMS_ADAPT_DT(sys_anim_sprite_adapt, sys_anim_sprite_impl)
