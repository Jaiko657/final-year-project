#pragma once
#include <stdbool.h>
#include "modules/core/build_config.h"

// Creates the window and sets target fps.
bool renderer_init(int width, int height, const char* title, int target_fps);
bool renderer_bind_world_map(void);
void renderer_unload_tiled_map(void);
void renderer_shutdown(void);
bool renderer_screen_to_world(float screen_x, float screen_y, float* out_x, float* out_y);

#if DEBUG_BUILD
bool renderer_toggle_ecs_colliders(void);
bool renderer_toggle_phys_colliders(void);
bool renderer_toggle_static_colliders(void);
bool renderer_toggle_triggers(void);
bool renderer_toggle_fps_overlay(void);
#else
static inline bool renderer_toggle_ecs_colliders(void){ return false; }
static inline bool renderer_toggle_phys_colliders(void){ return false; }
static inline bool renderer_toggle_static_colliders(void){ return false; }
static inline bool renderer_toggle_triggers(void){ return false; }
static inline bool renderer_toggle_fps_overlay(void){ return false; }
#endif
