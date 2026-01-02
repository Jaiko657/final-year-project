#include "unity.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "modules/core/cmp_print.h"
#include "modules/ecs/ecs_internal.h"
#include "modules/core/logger.h"

static char s_last_log[512];
static log_level_t s_last_level;
static const char* s_last_cat_name;

static void cmp_print_test_sink(log_level_t lvl, const log_cat_t* cat, const char* fmt, va_list ap)
{
    vsnprintf(s_last_log, sizeof(s_last_log), fmt, ap);
    s_last_level = lvl;
    s_last_cat_name = cat && cat->name ? cat->name : NULL;
}

void setUp(void)
{
    log_set_sink(cmp_print_test_sink);
    s_last_log[0] = '\0';
    s_last_cat_name = NULL;
    s_last_level = LOG_LVL_TRACE;
}

void tearDown(void)
{
    log_set_sink(NULL);
}

static void assert_last_log(const char* expected)
{
    TEST_ASSERT_EQUAL_STRING(expected, s_last_log);
    TEST_ASSERT_EQUAL_INT(LOG_LVL_INFO, s_last_level);
    TEST_ASSERT_EQUAL_STRING("ECS", s_last_cat_name);
}

void test_cmp_print_handles_nulls(void)
{
    cmp_print_position(NULL, NULL);
    assert_last_log("POS(null)");

    cmp_print_velocity(NULL, NULL);
    assert_last_log("VEL(null)");

    cmp_print_phys_body(NULL, NULL);
    assert_last_log("PHYS_BODY(null)");

    cmp_print_sprite(NULL, NULL);
    assert_last_log("SPR(null)");

    cmp_print_anim(NULL, NULL);
    assert_last_log("ANIM(null)");

    cmp_print_follow(NULL, NULL);
    assert_last_log("FOLLOW(null)");

    cmp_print_collider(NULL, NULL);
    assert_last_log("COL(null)");

    cmp_print_grav_gun(NULL, NULL);
    assert_last_log("GRAV_GUN(null)");

    cmp_print_trigger(NULL, NULL);
    assert_last_log("TRIGGER(null)");

    cmp_print_billboard(NULL, NULL);
    assert_last_log("BILLBOARD(null)");

    cmp_print_door(NULL, NULL);
    assert_last_log("DOOR(null)");
}

void test_cmp_print_position_and_velocity_formats(void)
{
    cmp_position_t pos = { .x = 1.25f, .y = -2.5f };
    cmp_print_position(NULL, &pos);
    assert_last_log("POS(x=1.250, y=-2.500)");

    cmp_velocity_t vel = {0};
    vel.x = 3.0f;
    vel.y = 4.0f;
    vel.facing.rawDir = DIR_NORTH;
    vel.facing.facingDir = DIR_EAST;
    vel.facing.candidateDir = DIR_SOUTHWEST;
    vel.facing.candidateTime = 0.75f;
    cmp_print_velocity(NULL, &vel);
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "VEL(x=3.000, y=4.000"));
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "raw=N"));
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "facing=E"));
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "cand=SW"));
    TEST_ASSERT_EQUAL_INT(LOG_LVL_INFO, s_last_level);
}

void test_cmp_print_phys_sprite_anim_formats(void)
{
    cmp_phys_body_t body = {0};
    body.type = PHYS_DYNAMIC;
    body.mass = 2.0f;
    body.inv_mass = 0.5f;
    body.category_bits = 0xAA;
    body.mask_bits = 0x55;
    body.created = true;
    cmp_print_phys_body(NULL, &body);
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "PHYS_BODY(type=DYN"));
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "cat=0xAA"));
    TEST_ASSERT_EQUAL_INT(LOG_LVL_INFO, s_last_level);

    cmp_sprite_t spr = {0};
    spr.tex.idx = 1;
    spr.tex.gen = 2;
    spr.src = rectf_xywh(1.0f, 2.0f, 3.0f, 4.0f);
    spr.ox = 0.5f;
    spr.oy = 1.5f;
    cmp_print_sprite(NULL, &spr);
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "SPR(tex=(1,2)"));
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "src=[1.0,2.0,3.0,4.0]"));
    TEST_ASSERT_EQUAL_INT(LOG_LVL_INFO, s_last_level);

    cmp_anim_t anim = {0};
    anim.frame_w = 16;
    anim.frame_h = 8;
    anim.current_anim = 1;
    anim.anim_count = 3;
    anim.frame_index = 2;
    anim.frame_duration = 0.2f;
    anim.current_time = 1.0f;
    cmp_print_anim(NULL, &anim);
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "ANIM(frame=16x8"));
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "anim=1/3"));
    TEST_ASSERT_EQUAL_INT(LOG_LVL_INFO, s_last_level);
}

void test_cmp_print_storage_format(void)
{
    cmp_print_player(NULL);
    assert_last_log("PLAYER()");

    cmp_print_storage(NULL, 3, 10);
    assert_last_log("STORAGE(plastic=3, capacity=10)");
}

void test_cmp_print_follow_collider_and_misc(void)
{
    cmp_follow_t follow = {0};
    follow.target.idx = 42;
    follow.desired_distance = 10.0f;
    follow.max_speed = 3.5f;
    follow.vision_range = 7.0f;
    follow.has_last_seen = false;
    cmp_print_follow(NULL, &follow);
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "FOLLOW(target=42"));
    TEST_ASSERT_TRUE(strstr(s_last_log, "last=") == NULL);
    TEST_ASSERT_EQUAL_INT(LOG_LVL_INFO, s_last_level);

    follow.has_last_seen = true;
    follow.last_seen_x = 1.0f;
    follow.last_seen_y = 2.0f;
    cmp_print_follow(NULL, &follow);
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "last=(1.0,2.0)"));
    TEST_ASSERT_EQUAL_INT(LOG_LVL_INFO, s_last_level);

    cmp_collider_t col = { .hx = 2.25f, .hy = 1.75f };
    cmp_print_collider(NULL, &col);
    assert_last_log("COL(hx=2.25, hy=1.75)");

    cmp_grav_gun_t grav = {0};
    grav.state = GRAV_GUN_STATE_HELD;
    grav.holder.idx = 9;
    grav.follow_gain = 2.0f;
    grav.max_speed = 50.0f;
    grav.hold_vel_x = 0.25f;
    grav.hold_vel_y = -0.75f;
    cmp_print_grav_gun(NULL, &grav);
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "GRAV_GUN(state=HELD"));
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "holder=9"));
    TEST_ASSERT_EQUAL_INT(LOG_LVL_INFO, s_last_level);

    cmp_trigger_t trig = { .pad = 1.25f, .target_mask = 0xAABBCCDD };
    cmp_print_trigger(NULL, &trig);
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "TRIGGER(pad=1.25"));
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "target_mask=0xAABBCCDD"));
    TEST_ASSERT_EQUAL_INT(LOG_LVL_INFO, s_last_level);

    cmp_billboard_t bb = {0};
    snprintf(bb.text, sizeof(bb.text), "abcdefghijklmnopqrstuvwxyz");
    bb.y_offset = 2.0f;
    bb.linger = 1.0f;
    bb.timer = 0.5f;
    bb.state = BILLBOARD_ACTIVE;
    cmp_print_billboard(NULL, &bb);
    const char *text_start = strstr(s_last_log, "text=\"");
    TEST_ASSERT_NOT_NULL(text_start);
    text_start += strlen("text=\"");
    const char *text_end = strchr(text_start, '"');
    TEST_ASSERT_NOT_NULL(text_end);
    TEST_ASSERT_EQUAL_INT(24, (int)(text_end - text_start));
    TEST_ASSERT_EQUAL_STRING_LEN("abcdefghijklmnopqrstuvwx", text_start, 24);

    cmp_door_t door = {0};
    door.prox_radius = 3.0f;
    door.state = DOOR_OPENING;
    door.anim_time_ms = 100.0f;
    door.intent_open = true;
    door.world_handle = 0xBEEF;
    cmp_print_door(NULL, &door);
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "DOOR(prox=3.0"));
    TEST_ASSERT_NOT_NULL(strstr(s_last_log, "state=OPENING"));
}
