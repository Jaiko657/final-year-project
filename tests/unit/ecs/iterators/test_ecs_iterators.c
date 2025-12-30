#include "unity.h"

#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs.h"
#include "modules/ecs/ecs_render.h"

#include <stdio.h>
#include <string.h>

static void reset_ecs_storage(void)
{
    memset(ecs_mask, 0, sizeof(ecs_mask));
    memset(ecs_gen, 0, sizeof(ecs_gen));
    memset(cmp_pos, 0, sizeof(cmp_pos));
    memset(cmp_spr, 0, sizeof(cmp_spr));
    memset(cmp_col, 0, sizeof(cmp_col));
    memset(cmp_trigger, 0, sizeof(cmp_trigger));
    memset(cmp_billboard, 0, sizeof(cmp_billboard));
    memset(cmp_phys_body, 0, sizeof(cmp_phys_body));
}

void setUp(void)
{
    reset_ecs_storage();
}

void tearDown(void)
{
}

void test_ecs_sprites_iterator_returns_sprite_views(void)
{
    ecs_gen[0] = 1;
    ecs_mask[0] = CMP_POS | CMP_SPR;
    cmp_pos[0] = (cmp_position_t){ 1.0f, 2.0f };
    cmp_spr[0].tex = (tex_handle_t){ 3, 4 };
    cmp_spr[0].src = rectf_xywh(0.0f, 0.0f, 8.0f, 8.0f);
    cmp_spr[0].ox = 1.0f;
    cmp_spr[0].oy = 2.0f;

    ecs_gen[1] = 1;
    ecs_mask[1] = CMP_POS | CMP_SPR;
    cmp_pos[1] = (cmp_position_t){ 5.0f, 6.0f };
    cmp_spr[1].tex = (tex_handle_t){ 7, 8 };

    ecs_sprite_iter_t it = ecs_sprites_begin();
    ecs_sprite_view_t view;

    TEST_ASSERT_TRUE(ecs_sprites_next(&it, &view));
    TEST_ASSERT_EQUAL_UINT32(3u, view.tex.idx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, view.x);

    TEST_ASSERT_TRUE(ecs_sprites_next(&it, &view));
    TEST_ASSERT_EQUAL_UINT32(7u, view.tex.idx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, view.x);

    TEST_ASSERT_FALSE(ecs_sprites_next(&it, &view));
}

void test_ecs_colliders_iterator_reports_phys_state(void)
{
    ecs_gen[0] = 1;
    ecs_mask[0] = CMP_POS | CMP_COL | CMP_PHYS_BODY;
    cmp_pos[0] = (cmp_position_t){ 2.0f, 3.0f };
    cmp_col[0] = (cmp_collider_t){ 4.0f, 5.0f };
    cmp_phys_body[0].created = true;

    ecs_collider_iter_t it = ecs_colliders_begin();
    ecs_collider_view_t view;

    TEST_ASSERT_TRUE(ecs_colliders_next(&it, &view));
    TEST_ASSERT_TRUE(view.has_phys);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, view.ecs_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, view.hx);
    TEST_ASSERT_FALSE(ecs_colliders_next(&it, &view));
}

void test_ecs_triggers_iterator_includes_collider_size(void)
{
    ecs_gen[0] = 1;
    ecs_mask[0] = CMP_POS | CMP_TRIGGER | CMP_COL;
    cmp_pos[0] = (cmp_position_t){ 1.0f, 1.0f };
    cmp_col[0] = (cmp_collider_t){ 2.0f, 3.0f };
    cmp_trigger[0] = (cmp_trigger_t){ 1.5f, CMP_ITEM };

    ecs_trigger_iter_t it = ecs_triggers_begin();
    ecs_trigger_view_t view;

    TEST_ASSERT_TRUE(ecs_triggers_next(&it, &view));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, view.hx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, view.hy);
}

void test_ecs_billboards_iterator_filters_and_fades(void)
{
    ecs_gen[0] = 1;
    ecs_mask[0] = CMP_POS | CMP_BILLBOARD;
    cmp_pos[0] = (cmp_position_t){ 1.0f, 2.0f };
    cmp_billboard[0].state = BILLBOARD_ACTIVE;
    cmp_billboard[0].timer = 1.0f;
    cmp_billboard[0].linger = 2.0f;
    cmp_billboard[0].y_offset = 3.0f;
    snprintf(cmp_billboard[0].text, sizeof(cmp_billboard[0].text), "hi");

    ecs_billboard_iter_t it = ecs_billboards_begin();
    ecs_billboard_view_t view;

    TEST_ASSERT_TRUE(ecs_billboards_next(&it, &view));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, view.alpha);
    TEST_ASSERT_EQUAL_STRING("hi", view.text);
}
