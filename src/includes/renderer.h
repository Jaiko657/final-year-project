#pragma once
#include <stdbool.h>

#ifndef DEBUG_COLLISION
#define DEBUG_COLLISION 1
#endif
#ifndef DEBUG_TRIGGERS
#define DEBUG_TRIGGERS 0
#endif
#ifndef DEBUG_FPS
#define DEBUG_FPS 1
#endif

typedef enum {
    COLLIDER_DEBUG_OFF = 0,
    COLLIDER_DEBUG_ECS,
    COLLIDER_DEBUG_PHYSICS,
    COLLIDER_DEBUG_BOTH,
    COLLIDER_DEBUG_MODE_COUNT
} collider_debug_mode_t;

void renderer_set_collider_debug_mode(collider_debug_mode_t mode);
collider_debug_mode_t renderer_get_collider_debug_mode(void);
const char* renderer_collider_debug_mode_label(collider_debug_mode_t mode);

// Creates the window and sets target fps.
bool renderer_init(int width, int height, const char* title, int target_fps);
bool renderer_load_tiled_map(const char* tmx_path);
void renderer_next_frame(void);
void renderer_shutdown(void);
