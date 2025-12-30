#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "modules/ecs/ecs.h"
#include "modules/systems/systems.h"
#include "modules/ecs/ecs_internal.h"
#include "modules/core/engine_types.h"
#include "modules/world/world_door.h"

extern int g_asset_acquire_calls;
extern int g_asset_release_calls;
extern int g_asset_addref_calls;
extern int g_asset_collect_calls;
extern bool g_asset_valid_result;

extern int g_phys_create_calls;
extern int g_phys_destroy_calls;

extern int g_world_door_register_calls;
extern int g_world_door_unregister_calls;
extern int g_world_door_last_count;
extern door_tile_xy_t g_world_door_last_tiles[4];

extern int g_world_resolve_mtv_calls;
extern int g_world_apply_edits_calls;

extern int g_ecs_register_system_calls;
extern int g_ecs_phase_calls[PHASE_COUNT];
extern int g_ecs_phase_order[16];
extern int g_ecs_phase_order_count;

extern int g_log_warn_calls;
extern int g_log_error_calls;

void ecs_core_stub_reset(void);
