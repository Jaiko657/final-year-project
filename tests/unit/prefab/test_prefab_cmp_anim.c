#include "unity.h"

#include <stddef.h>

#include "modules/prefab/prefab_cmp.h"

static prefab_component_t make_anim_comp(const char* type_name, prefab_anim_def_t* anim)
{
    return (prefab_component_t){
        .id = ENUM_COMPONENT_COUNT,
        .type_name = (char*)type_name,
        .props = NULL,
        .prop_count = 0,
        .anim = anim,
        .override_after_spawn = false,
    };
}

void test_prefab_cmp_anim_build_flattens_sequences(void)
{
    anim_frame_coord_t walk_frames[] = { {0, 0}, {1, 0} };
    anim_frame_coord_t idle_frames[] = { {2, 1} };
    prefab_anim_seq_t seqs[] = {
        { .name = (char*)"walk", .frames = walk_frames, .frame_count = 2 },
        { .name = (char*)"idle", .frames = idle_frames, .frame_count = 1 },
    };
    prefab_anim_def_t anim = {
        .frame_w = 16,
        .frame_h = 17,
        .fps = 6.0f,
        .seqs = seqs,
        .seq_count = 2,
    };
    prefab_component_t comp = make_anim_comp("ANIM", &anim);

    prefab_cmp_anim_t out = {0};
    TEST_ASSERT_TRUE(prefab_cmp_anim_build(&comp, NULL, &out));
    TEST_ASSERT_EQUAL_INT(16, out.frame_w);
    TEST_ASSERT_EQUAL_INT(17, out.frame_h);
    TEST_ASSERT_EQUAL_FLOAT(6.0f, out.fps);
    TEST_ASSERT_EQUAL_INT(2, out.anim_count);
    TEST_ASSERT_EQUAL_INT(2, out.frame_buffer_width);
    TEST_ASSERT_EQUAL_INT(2, out.frame_buffer_height);
    TEST_ASSERT_EQUAL_INT(2, out.frames_per_anim[0]);
    TEST_ASSERT_EQUAL_INT(1, out.frames_per_anim[1]);
    const anim_frame_coord_t* walk_00 = prefab_cmp_anim_frame_coord(&out, 0, 0);
    const anim_frame_coord_t* walk_01 = prefab_cmp_anim_frame_coord(&out, 0, 1);
    const anim_frame_coord_t* idle_00 = prefab_cmp_anim_frame_coord(&out, 1, 0);
    TEST_ASSERT_NOT_NULL(walk_00);
    TEST_ASSERT_NOT_NULL(walk_01);
    TEST_ASSERT_NOT_NULL(idle_00);
    TEST_ASSERT_EQUAL_INT(0, walk_00->col);
    TEST_ASSERT_EQUAL_INT(0, walk_00->row);
    TEST_ASSERT_EQUAL_INT(1, walk_01->col);
    TEST_ASSERT_EQUAL_INT(0, walk_01->row);
    TEST_ASSERT_EQUAL_INT(2, idle_00->col);
    TEST_ASSERT_EQUAL_INT(1, idle_00->row);
    prefab_cmp_anim_free(&out);
}
