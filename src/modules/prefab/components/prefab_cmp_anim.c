#include "modules/prefab/prefab_cmp.h"
#include <stdlib.h>
#include <string.h>

void prefab_cmp_anim_free(prefab_cmp_anim_t* anim)
{
    if (!anim) return;
    free(anim->frames_per_anim);
    free(anim->frames);
    memset(anim, 0, sizeof(*anim));
}

bool prefab_cmp_anim_build(const prefab_component_t* comp, const tiled_object_t* obj, prefab_cmp_anim_t* out_anim)
{
    if (!out_anim) return false;
    if (!comp || !comp->anim) return false;

    memset(out_anim, 0, sizeof(*out_anim));
    int frame_w = comp->anim->frame_w;
    int frame_h = comp->anim->frame_h;
    float fps = comp->anim->fps;
    prefab_parse_int(prefab_combined_value(comp, obj, "frame_w"), &frame_w);
    prefab_parse_int(prefab_combined_value(comp, obj, "frame_h"), &frame_h);
    prefab_parse_float(prefab_combined_value(comp, obj, "fps"), &fps);

    int anim_count = (int)comp->anim->seq_count;
    if (anim_count < 0) anim_count = 0;
    if (anim_count > MAX_ANIMS) anim_count = MAX_ANIMS;

    out_anim->frame_w = frame_w;
    out_anim->frame_h = frame_h;
    out_anim->fps = fps;
    out_anim->anim_count = anim_count;
    out_anim->frame_buffer_height = anim_count;

    if (anim_count == 0) {
        out_anim->frame_buffer_width = 0;
        return true;
    }

    int* counts = (int*)malloc((size_t)anim_count * sizeof(int));
    if (!counts) return false;
    out_anim->frames_per_anim = counts;

    int max_frames = 0;
    for (int i = 0; i < anim_count; ++i) {
        const prefab_anim_seq_t* seq = &comp->anim->seqs[i];
        int count = (int)seq->frame_count;
        if (count < 0) count = 0;
        counts[i] = count;
        if (count > max_frames) max_frames = count;
    }

    out_anim->frame_buffer_width = max_frames;

    size_t total_slots = (size_t)anim_count * (size_t)max_frames;
    if (total_slots > 0) {
        anim_frame_coord_t* frames = (anim_frame_coord_t*)malloc(total_slots * sizeof(anim_frame_coord_t));
        if (!frames) {
            prefab_cmp_anim_free(out_anim);
            return false;
        }
        out_anim->frames = frames;
    }

    for (int i = 0; i < anim_count; ++i) {
        const prefab_anim_seq_t* seq = &comp->anim->seqs[i];
        int count = counts[i];
        for (int f = 0; f < count; ++f) {
            anim_frame_coord_t* dst = prefab_cmp_anim_frame_coord_mut(out_anim, i, f);
            if (dst && seq->frames) {
                *dst = seq->frames[f];
            }
        }
    }

    return true;
}
