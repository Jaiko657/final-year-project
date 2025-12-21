#include "ecs_stubs.h"

static ecs_entity_t g_target = {0};
static bool g_has_pos = false;
static v2f g_pos = {0};

void ecs_stub_reset(void)
{
    g_target = (ecs_entity_t){0};
    g_has_pos = false;
    g_pos = (v2f){0};
}

void ecs_stub_set_target(ecs_entity_t target)
{
    g_target = target;
}

void ecs_stub_set_position(bool has_position, v2f position)
{
    g_has_pos = has_position;
    g_pos = position;
}

bool ecs_get_position(ecs_entity_t e, v2f* out_pos)
{
    if (!out_pos) return false;
    if (!g_has_pos) return false;
    if (e.idx != g_target.idx || e.gen != g_target.gen) return false;
    *out_pos = g_pos;
    return true;
}

