#pragma once

#include <stdbool.h>

#ifdef NOB_IMPLEMENTATION
#define NOB_IMPLEMENTATION_WAS_DEFINED
#undef NOB_IMPLEMENTATION
#endif

#include "../../../third_party/nob.h"

#ifdef NOB_IMPLEMENTATION_WAS_DEFINED
#define NOB_IMPLEMENTATION
#undef NOB_IMPLEMENTATION_WAS_DEFINED
#endif

bool generate_unity_runner(const char *group, const Nob_File_Paths *test_sources, const char *out_path);
