#include "unity.h"

#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs_proximity.h"
#include "modules/core/input.h"

#include <string.h>

void sys_prox_build_adapt(float dt, const input_t* in);
void sys_billboards_adapt(float dt, const input_t* in);

static void reset_storage(void)
{
    memset(ecs_mask, 0, sizeof(ecs_mask));
    memset(ecs_gen, 0, sizeof(ecs_gen));
    memset(cmp_pos, 0, sizeof(cmp_pos));
    memset(cmp_col, 0, sizeof(cmp_col));
    memset(cmp_trigger, 0, sizeof(cmp_trigger));
    memset(cmp_billboard, 0, sizeof(cmp_billboard));
}

void setUp(void)
{
    reset_storage();
}

void tearDown(void)
{
}

void test_proximity_enter_stay_exit(void)
{
    ecs_gen[0] = 1;
    ecs_mask[0] = CMP_POS | CMP_COL | CMP_TRIGGER | CMP_BILLBOARD;
    cmp_pos[0] = (cmp_position_t){ 0.0f, 0.0f };
    cmp_col[0] = (cmp_collider_t){ 1.0f, 1.0f };
    cmp_trigger[0] = (cmp_trigger_t){ 0.0f, CMP_PLASTIC };
    cmp_billboard[0].state = BILLBOARD_ACTIVE;
    cmp_billboard[0].linger = 2.0f;
    cmp_billboard[0].timer = 0.0f;

    ecs_gen[1] = 1;
    ecs_mask[1] = CMP_POS | CMP_COL | CMP_PLASTIC | CMP_GRAV_GUN;
    cmp_pos[1] = (cmp_position_t){ 0.5f, 0.5f };
    cmp_col[1] = (cmp_collider_t){ 1.0f, 1.0f };
    cmp_grav_gun[1].state = GRAV_GUN_STATE_HELD;

    sys_prox_build_adapt(0.0f, NULL);

    ecs_prox_iter_t it = ecs_prox_enter_begin();
    ecs_prox_view_t v;
    TEST_ASSERT_TRUE(ecs_prox_enter_next(&it, &v));
    TEST_ASSERT_FALSE(ecs_prox_enter_next(&it, &v));

    sys_billboards_adapt(0.1f, NULL);
    TEST_ASSERT_EQUAL_INT(BILLBOARD_ACTIVE, cmp_billboard[0].state);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, cmp_billboard[0].timer);

    sys_prox_build_adapt(0.0f, NULL);
    it = ecs_prox_enter_begin();
    TEST_ASSERT_FALSE(ecs_prox_enter_next(&it, &v));

    ecs_prox_iter_t stay = ecs_prox_stay_begin();
    TEST_ASSERT_TRUE(ecs_prox_stay_next(&stay, &v));

    cmp_pos[1] = (cmp_position_t){ 10.0f, 10.0f };
    sys_prox_build_adapt(0.0f, NULL);

    ecs_prox_iter_t exit_it = ecs_prox_exit_begin();
    TEST_ASSERT_TRUE(ecs_prox_exit_next(&exit_it, &v));
}
