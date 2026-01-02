#pragma once

#include <stddef.h>

#include "modules/ecs/ecs.h"
#include "modules/ecs/ecs_proximity.h"
#include "modules/systems/systems.h"
#include "modules/tiled/tiled.h"

void ecs_game_stub_set_player(ecs_entity_t e);
void ecs_game_stub_reset(void);
void ecs_game_stub_set_prox_enter(const ecs_prox_view_t* views, size_t count);
void ecs_game_stub_set_prox_stay(const ecs_prox_view_t* views, size_t count);

extern int g_prefab_spawn_calls;
extern char g_prefab_spawn_last_path[256];
extern int g_ecs_register_system_calls;
extern int g_world_door_apply_calls;
extern int g_world_door_anim_calls;
extern int g_world_door_primary_duration;
extern float g_world_door_last_time;
extern bool g_world_door_last_forward;
extern int g_ui_toast_calls;
extern char g_ui_toast_last[128];
extern const world_map_t* g_world_tiled_map;

extern systems_fn g_ecs_sys_storage;
extern systems_fn g_ecs_sys_doors_tick;
