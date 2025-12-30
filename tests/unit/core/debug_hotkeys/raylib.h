#pragma once

#include <stdbool.h>

typedef struct Vector2 { float x; float y; } Vector2;

typedef struct Camera2D {
    Vector2 offset;
    Vector2 target;
    float rotation;
    float zoom;
} Camera2D;

int GetScreenWidth(void);
int GetScreenHeight(void);
Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera);

void raylib_debug_stub_reset(void);
void raylib_debug_stub_set_screen(int w, int h);
