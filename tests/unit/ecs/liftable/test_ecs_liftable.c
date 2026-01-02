#include "unity.h"

#include "modules/ecs/ecs_internal.h"
#include "modules/core/input.h"
#include "ecs_liftable_stubs.h"

#include <string.h>

void sys_grav_gun_input_adapt(float dt, const input_t* in);
void sys_grav_gun_motion_adapt(float dt, const input_t* in);

void setUp(void)
{
    memset(ecs_mask, 0, sizeof(ecs_mask));
    memset(ecs_gen, 0, sizeof(ecs_gen));
    memset(cmp_pos, 0, sizeof(cmp_pos));
    memset(cmp_vel, 0, sizeof(cmp_vel));
    memset(cmp_grav_gun, 0, sizeof(cmp_grav_gun));
    memset(cmp_phys_body, 0, sizeof(cmp_phys_body));
    memset(cmp_col, 0, sizeof(cmp_col));
}

void tearDown(void)
{
}

void test_grav_gun_grab_and_release(void)
{
    ecs_gen[0] = 1;
    ecs_mask[0] = CMP_POS | CMP_PLAYER | CMP_PHYS_BODY;
    cmp_pos[0] = (cmp_position_t){ 0.0f, 0.0f };
    cmp_phys_body[0].category_bits = PHYS_CAT_PLAYER;

    ecs_gen[1] = 1;
    ecs_mask[1] = CMP_POS | CMP_PHYS_BODY | CMP_COL | CMP_GRAV_GUN;
    cmp_pos[1] = (cmp_position_t){ 10.0f, 0.0f };
    cmp_col[1] = (cmp_collider_t){ 3.0f, 3.0f };
    cmp_phys_body[1].type = PHYS_DYNAMIC;
    cmp_grav_gun[1].state = GRAV_GUN_STATE_FREE;
    cmp_grav_gun[1].pickup_distance = 20.0f;
    cmp_grav_gun[1].pickup_radius = 5.0f;

    grav_gun_stub_set_player((ecs_entity_t){0, 1});

    input_t in = {0};
    in.down = (1ull << BTN_MOUSE_L);
    in.pressed = (1ull << BTN_MOUSE_L);
    in.mouse.x = 10.0f;
    in.mouse.y = 0.0f;
    sys_grav_gun_input_adapt(0.0f, &in);

    TEST_ASSERT_EQUAL_INT(GRAV_GUN_STATE_HELD, cmp_grav_gun[1].state);
    TEST_ASSERT_EQUAL_UINT32(0u, cmp_grav_gun[1].holder.idx);
    TEST_ASSERT_TRUE(cmp_grav_gun[1].saved_mask_valid);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu & ~PHYS_CAT_PLAYER, cmp_phys_body[1].mask_bits);

    input_t release = {0};
    sys_grav_gun_input_adapt(0.0f, &release);

    TEST_ASSERT_EQUAL_INT(GRAV_GUN_STATE_FREE, cmp_grav_gun[1].state);
    TEST_ASSERT_FALSE(cmp_grav_gun[1].saved_mask_valid);
    TEST_ASSERT_EQUAL_UINT32(0u, cmp_phys_body[1].mask_bits);
}

void test_grav_gun_motion_updates_velocity(void)
{
    ecs_gen[0] = 1;
    ecs_mask[0] = CMP_POS | CMP_PLAYER | CMP_PHYS_BODY;
    cmp_pos[0] = (cmp_position_t){ 0.0f, 0.0f };

    ecs_gen[1] = 1;
    ecs_mask[1] = CMP_POS | CMP_PHYS_BODY | CMP_GRAV_GUN;
    cmp_pos[1] = (cmp_position_t){ 0.0f, 0.0f };
    cmp_phys_body[1].type = PHYS_DYNAMIC;
    cmp_grav_gun[1].state = GRAV_GUN_STATE_HELD;
    cmp_grav_gun[1].holder = (ecs_entity_t){0, 1};
    cmp_grav_gun[1].max_hold_distance = 50.0f;
    cmp_grav_gun[1].follow_gain = 5.0f;
    cmp_grav_gun[1].max_speed = 100.0f;
    cmp_grav_gun[1].damping = 1.0f;

    input_t in = {0};
    in.down = (1ull << BTN_MOUSE_L);
    in.mouse.x = 20.0f;
    in.mouse.y = 0.0f;
    sys_grav_gun_motion_adapt(1.0f, &in);

    TEST_ASSERT_TRUE((ecs_mask[1] & CMP_VEL) != 0);
    TEST_ASSERT_TRUE(cmp_vel[1].x > 0.0f);
    TEST_ASSERT_EQUAL_INT(GRAV_GUN_STATE_HELD, cmp_grav_gun[1].state);
}
