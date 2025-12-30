#include "unity.h"

#include <string.h>

#include "modules/systems/systems.h"

static int g_calls[16];
static int g_call_count = 0;

static void record_call(int id)
{
    if (g_call_count < (int)(sizeof(g_calls)/sizeof(g_calls[0]))) {
        g_calls[g_call_count++] = id;
    }
}

static void sys_a(float dt, const input_t* in) { (void)dt; (void)in; record_call(1); }
static void sys_b(float dt, const input_t* in) { (void)dt; (void)in; record_call(2); }
static void sys_c(float dt, const input_t* in) { (void)dt; (void)in; record_call(3); }

void test_ecs_systems_run_phase_orders_by_order_field(void)
{
    systems_init();
    g_call_count = 0;

    systems_register(PHASE_SIM_PRE, 200, sys_b, "b");
    systems_register(PHASE_SIM_PRE, 100, sys_a, "a");
    systems_register(PHASE_SIM_PRE, 300, sys_c, "c");

    systems_run_phase(PHASE_SIM_PRE, 0.0f, NULL);

    TEST_ASSERT_EQUAL_INT(3, g_call_count);
    TEST_ASSERT_EQUAL_INT(1, g_calls[0]);
    TEST_ASSERT_EQUAL_INT(2, g_calls[1]);
    TEST_ASSERT_EQUAL_INT(3, g_calls[2]);
}

void test_ecs_systems_get_phase_systems_returns_sorted_list(void)
{
    systems_init();

    systems_register(PHASE_PHYSICS, 50, sys_b, "b");
    systems_register(PHASE_PHYSICS, 10, sys_a, "a");

    const systems_info_t* list = NULL;
    size_t n = systems_get_phase_systems(PHASE_PHYSICS, &list);

    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)n);
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL_STRING("a", list[0].name);
    TEST_ASSERT_EQUAL_INT(10, list[0].order);
    TEST_ASSERT_EQUAL_STRING("b", list[1].name);
    TEST_ASSERT_EQUAL_INT(50, list[1].order);
}

void test_ecs_systems_caps_system_count(void)
{
    systems_init();

    // Default cap is 64 in systems.c.
    for (int i = 0; i < 100; ++i) {
        systems_register(PHASE_INPUT, i, sys_a, "x");
    }

    const systems_info_t* list = NULL;
    size_t n = systems_get_phase_systems(PHASE_INPUT, &list);
    TEST_ASSERT_TRUE(n <= 64);
}
