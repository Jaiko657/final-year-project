#include "modules/core/platform.h"

#include "raylib.h"

void platform_init(void) { }

inline bool platform_should_close(void)
{
    return WindowShouldClose();
}
