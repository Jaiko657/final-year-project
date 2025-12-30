#pragma once

#include <stdbool.h>

#include "modules/ecs/ecs.h"

extern int g_asset_reload_calls;
extern int g_asset_log_calls;
extern int g_renderer_ecs_calls;
extern int g_renderer_phys_calls;
extern int g_renderer_static_calls;
extern int g_renderer_triggers_calls;
extern int g_renderer_fps_calls;
extern int g_toast_calls;
extern char g_last_toast[256];
extern int g_log_calls;
extern bool g_engine_reload_world_result;
extern int g_world_tiles_w;
extern int g_world_tiles_h;
extern bool g_ecs_alive[ECS_MAX_ENTITIES];
extern int g_game_item_kind;
extern int g_game_inv_coins;
extern bool g_game_inv_hat;
extern int g_game_vendor_sells;
extern int g_game_vendor_price;

void debug_hotkeys_stub_reset(void);
