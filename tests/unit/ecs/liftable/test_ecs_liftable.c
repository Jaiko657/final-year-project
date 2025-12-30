#include "unity.h"

#include "modules/ecs/ecs_internal.h"
#include "modules/core/input.h"
#include "ecs_liftable_stubs.h"

#include <string.h>

void sys_liftable_input_adapt(float dt, const input_t* in);
void sys_liftable_motion_adapt(float dt, const input_t* in);

void setUp(void)
{
    memset(ecs_mask, 0, sizeof(ecs_mask));
    memset(ecs_gen, 0, sizeof(ecs_gen));
    memset(cmp_pos, 0, sizeof(cmp_pos));
    memset(cmp_vel, 0, sizeof(cmp_vel));
    memset(cmp_liftable, 0, sizeof(cmp_liftable));
}

void tearDown(void)
{
}

void test_liftable_pickup_and_throw(void)
{
    ecs_gen[0] = 1;
    ecs_mask[0] = CMP_POS | CMP_VEL;
    cmp_pos[0] = (cmp_position_t){ 0.0f, 0.0f };
    cmp_vel[0].facing.facingDir = DIR_EAST;

    ecs_gen[1] = 1;
    ecs_mask[1] = CMP_POS | CMP_LIFTABLE;
    cmp_pos[1] = (cmp_position_t){ 5.0f, 0.0f };
    cmp_liftable[1].state = LIFTABLE_STATE_ONGROUND;
    cmp_liftable[1].pickup_distance = 10.0f;
    cmp_liftable[1].pickup_radius = 20.0f;
    cmp_liftable[1].carry_distance = 5.0f;
    cmp_liftable[1].carry_height = 3.0f;
    cmp_liftable[1].throw_speed = 10.0f;

    liftable_stub_set_player((ecs_entity_t){0, 1});

    input_t in = {0};
    in.pressed = (1ull << BTN_LIFT);
    sys_liftable_input_adapt(0.0f, &in);

    TEST_ASSERT_EQUAL_INT(LIFTABLE_STATE_CARRIED, cmp_liftable[1].state);
    TEST_ASSERT_EQUAL_UINT32(0u, cmp_liftable[1].carrier.idx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, cmp_pos[1].x);

    sys_liftable_input_adapt(0.0f, &in);
    TEST_ASSERT_EQUAL_INT(LIFTABLE_STATE_THROWN, cmp_liftable[1].state);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)-1, cmp_liftable[1].carrier.idx);
    TEST_ASSERT_TRUE(cmp_liftable[1].vx > 0.0f);
}

void test_liftable_motion_updates_thrown_entity(void)
{
    ecs_gen[1] = 1;
    ecs_mask[1] = CMP_POS | CMP_LIFTABLE;
    cmp_pos[1] = (cmp_position_t){ 1.0f, 10.0f };
    cmp_liftable[1].state = LIFTABLE_STATE_THROWN;
    cmp_liftable[1].vx = 10.0f;
    cmp_liftable[1].vy = -5.0f;
    cmp_liftable[1].height = 1.0f;
    cmp_liftable[1].gravity = 0.0f;

    sys_liftable_motion_adapt(1.0f, NULL);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 11.0f, cmp_pos[1].x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, cmp_pos[1].y);
    TEST_ASSERT_EQUAL_INT(LIFTABLE_STATE_THROWN, cmp_liftable[1].state);
}
