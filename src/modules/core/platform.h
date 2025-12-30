#pragma once
#include <stdbool.h>

// Headless builds may need to normalize the working directory for relative asset paths.
void platform_init(void);

// True when the host wants to exit the main loop.
bool platform_should_close(void);
