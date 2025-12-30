#include "unity.h"

#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs_physics.h"

#include <string.h>

void setUp(void)
{
    memset(ecs_mask, 0, sizeof(ecs_mask));
    memset(ecs_gen, 0, sizeof(ecs_gen));
    memset(cmp_phys_body, 0, sizeof(cmp_phys_body));
}

void tearDown(void)
{
}

void test_ecs_phys_body_create_requires_components(void)
{
    ecs_gen[0] = 1;
    ecs_mask[0] = CMP_POS | CMP_COL | CMP_PHYS_BODY;

    ecs_phys_body_create_for_entity(0);
    TEST_ASSERT_TRUE(cmp_phys_body[0].created);
}

void test_ecs_phys_body_destroy_clears_flag(void)
{
    cmp_phys_body[0].created = true;
    ecs_phys_body_destroy_for_entity(0);
    TEST_ASSERT_FALSE(cmp_phys_body[0].created);
}

void test_ecs_phys_destroy_all_visits_alive_phys_bodies(void)
{
    ecs_gen[0] = 1;
    ecs_mask[0] = CMP_PHYS_BODY;
    cmp_phys_body[0].created = true;

    ecs_gen[1] = 1;
    ecs_mask[1] = CMP_PHYS_BODY;
    cmp_phys_body[1].created = true;

    ecs_phys_destroy_all();
    TEST_ASSERT_FALSE(cmp_phys_body[0].created);
    TEST_ASSERT_FALSE(cmp_phys_body[1].created);
}
