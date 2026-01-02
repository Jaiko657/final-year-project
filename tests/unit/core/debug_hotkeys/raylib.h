#pragma once

#include <stdbool.h>

typedef struct Vector2 { float x; float y; } Vector2;

typedef struct Camera2D {
    Vector2 offset;
    Vector2 target;
    float rotation;
    float zoom;
} Camera2D;

typedef enum KeyboardKey {
    KEY_PRINT_SCREEN = 0
} KeyboardKey;

int GetScreenWidth(void);
int GetScreenHeight(void);
Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera);
bool IsKeyPressed(int key);
bool DirectoryExists(const char* dir);
bool MakeDirectory(const char* dir);
bool FileExists(const char* fileName);
void TakeScreenshot(const char* fileName);

void raylib_debug_stub_reset(void);
void raylib_debug_stub_set_screen(int w, int h);
