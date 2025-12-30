#pragma once

#include <stdbool.h>
#include "modules/ecs/ecs.h"
#include "modules/prefab/prefab.h"
#include "modules/prefab/prefab_cmp.h"

extern int g_cmp_add_position_calls;
extern int g_cmp_add_size_calls;
extern float g_cmp_add_size_last_hx;
extern float g_cmp_add_size_last_hy;
extern int g_cmp_add_follow_calls;
extern ecs_entity_t g_cmp_add_follow_last_target;
extern float g_cmp_add_follow_last_distance;
extern float g_cmp_add_follow_last_speed;
extern int g_prefab_load_calls;
extern char g_prefab_load_path[256];
extern bool g_prefab_load_result;
extern int g_log_warn_calls;

extern bool g_prefab_cmp_follow_result;
extern prefab_cmp_follow_t g_prefab_cmp_follow_out;

void ecs_prefab_loading_stub_reset(void);
void ecs_prefab_loading_stub_set_player(ecs_entity_t e);
