#pragma once
#include <stdbool.h>

bool engine_init(const char *title);
// returns process exit code (0 OK, non-zero error)
int engine_run(void);
void engine_shutdown(void);
bool engine_reload_world(void);
bool engine_reload_world_from_path(const char* tmx_path);
