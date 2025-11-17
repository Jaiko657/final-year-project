#pragma once
#include <stdbool.h>

bool engine_init(int width, int height, const char *title);
// returns process exit code (0 OK, non-zero error)
int engine_run(void);
void engine_shutdown(void);