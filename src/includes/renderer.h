#pragma once
#include <stdbool.h>
#include "build_config.h"

#include "tiled.h"

// Creates the window and sets target fps.
bool renderer_init(int width, int height, const char* title, int target_fps);
bool renderer_load_tiled_map(const char* tmx_path);
void renderer_unload_tiled_map(void);
void renderer_next_frame(void);
void renderer_shutdown(void);

tiled_map_t* renderer_get_tiled_map(void);

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
