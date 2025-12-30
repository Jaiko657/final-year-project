#include "unity.h"

#include "modules/ecs/ecs.h"
#include "modules/ecs/ecs_internal.h"
#include "modules/asset/bump_alloc.h"
#include "modules/core/input.h"

#include <string.h>

void ecs_anim_stub_set_player(ecs_entity_t e);
void sys_anim_controller_adapt(float dt, const input_t* in);
void sys_anim_sprite_adapt(float dt, const input_t* in);

void setUp(void)
{
    memset(ecs_mask, 0, sizeof(ecs_mask));
    memset(ecs_gen, 0, sizeof(ecs_gen));
    memset(cmp_anim, 0, sizeof(cmp_anim));
    memset(cmp_vel, 0, sizeof(cmp_vel));
    memset(cmp_spr, 0, sizeof(cmp_spr));
    ecs_anim_reset_allocator();
}

void tearDown(void)
{
    ecs_anim_shutdown_allocator();
}

void test_cmp_add_anim_sets_fields_and_mask(void)
{
    ecs_gen[0] = 1;
    ecs_entity_t e = {0, 1};

    int frames_per_anim[1] = {2};
    const int frame_buffer_width = 2;
    anim_frame_coord_t frames[2] = {
        [0] = (anim_frame_coord_t){ .col = 1, .row = 2 },
        [1] = (anim_frame_coord_t){ .col = 3, .row = 4 },
    };

    cmp_add_anim(e, 16, 8, 1, frames_per_anim, frames, frame_buffer_width, 4.0f);

    TEST_ASSERT_TRUE(ecs_mask[0] & CMP_ANIM);
    TEST_ASSERT_EQUAL_INT(16, cmp_anim[0].frame_w);
    TEST_ASSERT_EQUAL_INT(8, cmp_anim[0].frame_h);
    TEST_ASSERT_EQUAL_INT(1, cmp_anim[0].anim_count);
    TEST_ASSERT_EQUAL_INT(0, cmp_anim[0].current_anim);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.25f, cmp_anim[0].frame_duration);
}

void test_cmp_add_anim_defaults_fps_when_non_positive(void)
{
    ecs_gen[0] = 1;
    ecs_entity_t e = {0, 1};

    int frames_per_anim[1] = {1};
    const int frame_buffer_width = 1;
    anim_frame_coord_t frames[1] = {
        [0] = (anim_frame_coord_t){ .col = 0, .row = 0 },
    };

    cmp_add_anim(e, 8, 8, 1, frames_per_anim, frames, frame_buffer_width, 0.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.1f, cmp_anim[0].frame_duration);
}

void test_sys_anim_controller_sets_idle_and_walk_anim(void)
{
    ecs_gen[0] = 1;
    ecs_entity_t player = {0, 1};
    ecs_anim_stub_set_player(player);

    int frames_per_anim[MAX_ANIMS];
    anim_frame_coord_t frames[MAX_ANIMS];
    for (int i = 0; i < MAX_ANIMS; ++i) {
        frames_per_anim[i] = 1;
        frames[i] = (anim_frame_coord_t){ .col = i, .row = 0 };
    }

    const int frame_buffer_width = 1;
    cmp_add_anim(player, 16, 16, MAX_ANIMS, frames_per_anim, frames, frame_buffer_width, 4.0f);
    ecs_mask[0] |= CMP_VEL;
    cmp_vel[0].facing.facingDir = DIR_EAST;
    cmp_vel[0].x = 0.0f;
    cmp_vel[0].y = 0.0f;

    sys_anim_controller_adapt(0.0f, NULL);
    TEST_ASSERT_EQUAL_INT(8 + DIR_EAST, cmp_anim[0].current_anim);

    cmp_vel[0].x = 1.0f;
    sys_anim_controller_adapt(0.0f, NULL);
    TEST_ASSERT_EQUAL_INT(DIR_EAST, cmp_anim[0].current_anim);
}

void test_sys_anim_controller_ignores_invalid_facing(void)
{
    ecs_gen[0] = 1;
    ecs_entity_t player = {0, 1};
    ecs_anim_stub_set_player(player);

    int frames_per_anim[1] = {1};
    const int frame_buffer_width = 1;
    anim_frame_coord_t frames[1] = {
        [0] = (anim_frame_coord_t){ .col = 0, .row = 0 },
    };

    cmp_add_anim(player, 8, 8, 1, frames_per_anim, frames, frame_buffer_width, 4.0f);
    ecs_mask[0] |= CMP_VEL;
    cmp_vel[0].facing.facingDir = (facing_t)30;
    cmp_anim[0].current_anim = 0;

    sys_anim_controller_adapt(0.0f, NULL);
    TEST_ASSERT_EQUAL_INT(0, cmp_anim[0].current_anim);
}

void test_sys_anim_sprite_advances_frame_and_updates_sprite(void)
{
    ecs_gen[0] = 1;
    ecs_entity_t e = {0, 1};

    int frames_per_anim[1] = {2};
    const int frame_buffer_width = 2;
    anim_frame_coord_t frames[2] = {
        [0] = (anim_frame_coord_t){ .col = 1, .row = 0 },
        [1] = (anim_frame_coord_t){ .col = 2, .row = 0 },
    };

    cmp_add_anim(e, 8, 8, 1, frames_per_anim, frames, frame_buffer_width, 1.0f);
    cmp_spr[0].src = rectf_xywh(0.0f, 0.0f, 8.0f, 8.0f);
    ecs_mask[0] |= CMP_SPR;

    sys_anim_sprite_adapt(1.1f, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 16.0f, cmp_spr[0].src.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmp_spr[0].src.y);
}

void test_cmp_add_anim_deduplicates_defs(void)
{
    ecs_gen[0] = 1;
    ecs_gen[1] = 1;
    ecs_entity_t e0 = {0, 1};
    ecs_entity_t e1 = {1, 1};

    int frames_per_anim[1] = {2};
    const int frame_buffer_width = 2;
    anim_frame_coord_t frames[2] = {
        [0] = (anim_frame_coord_t){ .col = 1, .row = 0 },
        [1] = (anim_frame_coord_t){ .col = 2, .row = 0 },
    };

    cmp_add_anim(e0, 8, 8, 1, frames_per_anim, frames, frame_buffer_width, 4.0f);
    const anim_frame_coord_t* def_frames = cmp_anim[0].frames;
    const int* def_counts = cmp_anim[0].frames_per_anim;

    cmp_add_anim(e1, 8, 8, 1, frames_per_anim, frames, frame_buffer_width, 4.0f);
    TEST_ASSERT_EQUAL_PTR(def_frames, cmp_anim[1].frames);
    TEST_ASSERT_EQUAL_PTR(def_counts, cmp_anim[1].frames_per_anim);
}

void test_sys_anim_sprite_ignores_invalid_anim_index(void)
{
    ecs_gen[0] = 1;
    ecs_entity_t e = {0, 1};

    int frames_per_anim[1] = {1};
    const int frame_buffer_width = 1;
    anim_frame_coord_t frames[1] = {
        [0] = (anim_frame_coord_t){ .col = 0, .row = 0 },
    };

    cmp_add_anim(e, 8, 8, 1, frames_per_anim, frames, frame_buffer_width, 2.0f);
    cmp_anim[0].current_anim = 2;
    cmp_anim[0].frame_index = 0;
    cmp_anim[0].current_time = 0.0f;
    cmp_spr[0].src = rectf_xywh(5.0f, 6.0f, 8.0f, 8.0f);
    ecs_mask[0] |= CMP_SPR;

    sys_anim_sprite_adapt(1.0f, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, cmp_spr[0].src.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, cmp_spr[0].src.y);
}
