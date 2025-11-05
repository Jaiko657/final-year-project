#pragma once
#include <stdbool.h>

#ifndef DEBUG_COLLISION
#define DEBUG_COLLISION 1
#endif
#ifndef DEBUG_FPS
#define DEBUG_FPS 1
#endif

// Creates the window and sets target fps.
bool renderer_init(int width, int height, const char* title, int target_fps);

// Runs one full render frame (begin/clear → draw → end).
void renderer_next_frame(void);

// Closes the window and cleans up anything renderer owns.
void renderer_shutdown(void);
