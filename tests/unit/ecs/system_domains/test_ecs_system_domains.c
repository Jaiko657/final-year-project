#include "unity.h"

#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs_physics.h"
#include "modules/core/input.h"

void sys_input(float dt, const input_t* in);
void sys_follow(float dt);
void sys_physics_integrate_impl(float dt);

void ecs_system_domains_stub_reset(void);
extern bool g_world_has_los;
extern bool g_world_walkable;
extern int g_world_resolve_axis_calls;
extern int g_phys_create_calls;

void setUp(void)
{
    ecs_system_domains_stub_reset();
}

void tearDown(void)
{
}

void test_sys_input_updates_velocity_and_facing(void)
{
    ecs_gen[0] = 1;
    ecs_mask[0] = CMP_PLAYER | CMP_VEL;
    cmp_vel[0].facing = (smoothed_facing_t){
        .rawDir = DIR_SOUTH,
        .facingDir = DIR_SOUTH,
        .candidateDir = DIR_SOUTH,
        .candidateTime = 0.0f
    };

    input_t in = {0};
    in.moveX = 1.0f;
    in.moveY = 0.0f;

    sys_input(0.05f, &in);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 120.0f, cmp_vel[0].x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmp_vel[0].y);
    TEST_ASSERT_EQUAL_INT(DIR_EAST, cmp_vel[0].facing.rawDir);
    TEST_ASSERT_EQUAL_INT(DIR_EAST, cmp_vel[0].facing.facingDir);
}

void test_sys_follow_sets_velocity_towards_target(void)
{
    ecs_gen[0] = 1;
    ecs_gen[1] = 1;
    ecs_mask[0] = CMP_FOLLOW | CMP_POS | CMP_VEL;
    ecs_mask[1] = CMP_POS;

    cmp_pos[0] = (cmp_position_t){ 0.0f, 0.0f };
    cmp_pos[1] = (cmp_position_t){ 10.0f, 0.0f };
    cmp_follow[0] = (cmp_follow_t){
        .target = (ecs_entity_t){ 1, 1 },
        .desired_distance = 0.0f,
        .max_speed = 5.0f,
        .vision_range = -1.0f
    };

    g_world_has_los = true;
    g_world_walkable = true;

    sys_follow(0.016f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, cmp_vel[0].x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmp_vel[0].y);
}

void test_sys_physics_integrate_applies_velocity_and_clears(void)
{
    ecs_gen[0] = 1;
    ecs_mask[0] = CMP_POS | CMP_VEL | CMP_COL | CMP_PHYS_BODY;
    cmp_pos[0] = (cmp_position_t){ 0.0f, 0.0f };
    cmp_vel[0] = (cmp_velocity_t){ 10.0f, 0.0f, {0} };
    cmp_col[0] = (cmp_collider_t){ 1.0f, 1.0f };
    cmp_phys_body[0] = (cmp_phys_body_t){ .type = PHYS_DYNAMIC, .mass = 1.0f, .inv_mass = 1.0f, .created = true };

    sys_physics_integrate_impl(1.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, cmp_pos[0].x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmp_pos[0].y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmp_vel[0].x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cmp_vel[0].y);
    TEST_ASSERT_EQUAL_INT(1, g_world_resolve_axis_calls);
}

void test_sys_physics_integrate_creates_missing_body(void)
{
    ecs_gen[0] = 1;
    ecs_mask[0] = CMP_POS | CMP_COL | CMP_PHYS_BODY;
    cmp_pos[0] = (cmp_position_t){ 0.0f, 0.0f };
    cmp_col[0] = (cmp_collider_t){ 1.0f, 1.0f };
    cmp_phys_body[0] = (cmp_phys_body_t){ .type = PHYS_DYNAMIC, .mass = 1.0f, .inv_mass = 1.0f, .created = false };

    sys_physics_integrate_impl(0.1f);

    TEST_ASSERT_EQUAL_INT(1, g_phys_create_calls);
    TEST_ASSERT_TRUE(cmp_phys_body[0].created);
}
