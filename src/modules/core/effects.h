#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "modules/core/engine_types.h"

enum { FX_MAX_LINES = 128 };

typedef struct {
    v2f a;
    v2f b;
    float width;
    colorf color;
} fx_line_t;

void fx_lines_clear(void);
bool fx_line_push(v2f a, v2f b, float width, colorf color);
size_t fx_line_count(void);
const fx_line_t* fx_line_at(size_t idx);
