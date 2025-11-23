#pragma once
#include <stdbool.h>

#ifndef DEBUG_COLLISION
#define DEBUG_COLLISION 0
#endif
#ifndef DEBUG_TRIGGERS
#define DEBUG_TRIGGERS 0
#endif
#ifndef DEBUG_FPS
#define DEBUG_FPS 1
#endif

// Creates the window and sets target fps.
bool renderer_init(int width, int height, const char* title, int target_fps);
void renderer_next_frame(void);
void renderer_shutdown(void);
