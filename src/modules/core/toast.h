#pragma once

#include <stdbool.h>

void ui_toast_init(void);
void ui_toast_update(float dt);

// Show a toast for `secs` seconds, printf-style
void ui_toast(float secs, const char* fmt, ...);

// Read-only access for rendering
bool        ecs_toast_is_active(void);
const char* ecs_toast_get_text(void);
