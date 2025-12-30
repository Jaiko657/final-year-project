#include "raylib.h"

#include <string.h>

static bool g_key_down[512];
static bool g_key_pressed[512];
static bool g_mouse_down[16];
static bool g_mouse_pressed[16];
static Vector2 g_mouse_pos;
static float g_mouse_wheel;

void raylib_stub_reset(void)
{
    memset(g_key_down, 0, sizeof(g_key_down));
    memset(g_key_pressed, 0, sizeof(g_key_pressed));
    memset(g_mouse_down, 0, sizeof(g_mouse_down));
    memset(g_mouse_pressed, 0, sizeof(g_mouse_pressed));
    g_mouse_pos = (Vector2){0};
    g_mouse_wheel = 0.0f;
}

void raylib_stub_set_key_down(int key, bool down)
{
    if (key >= 0 && key < (int)(sizeof(g_key_down) / sizeof(g_key_down[0]))) {
        g_key_down[key] = down;
    }
}

void raylib_stub_set_key_pressed(int key, bool pressed)
{
    if (key >= 0 && key < (int)(sizeof(g_key_pressed) / sizeof(g_key_pressed[0]))) {
        g_key_pressed[key] = pressed;
    }
}

void raylib_stub_set_mouse_down(int button, bool down)
{
    int idx = button - MOUSE_BUTTON_LEFT;
    if (idx >= 0 && idx < (int)(sizeof(g_mouse_down) / sizeof(g_mouse_down[0]))) {
        g_mouse_down[idx] = down;
    }
}

void raylib_stub_set_mouse_pressed(int button, bool pressed)
{
    int idx = button - MOUSE_BUTTON_LEFT;
    if (idx >= 0 && idx < (int)(sizeof(g_mouse_pressed) / sizeof(g_mouse_pressed[0]))) {
        g_mouse_pressed[idx] = pressed;
    }
}

void raylib_stub_set_mouse_pos(float x, float y)
{
    g_mouse_pos = (Vector2){x, y};
}

void raylib_stub_set_mouse_wheel(float delta)
{
    g_mouse_wheel = delta;
}

bool IsKeyDown(int key)
{
    if (key < 0 || key >= (int)(sizeof(g_key_down) / sizeof(g_key_down[0]))) return false;
    return g_key_down[key];
}

bool IsKeyPressed(int key)
{
    if (key < 0 || key >= (int)(sizeof(g_key_pressed) / sizeof(g_key_pressed[0]))) return false;
    return g_key_pressed[key];
}

bool IsMouseButtonDown(int button)
{
    int idx = button - MOUSE_BUTTON_LEFT;
    if (idx < 0 || idx >= (int)(sizeof(g_mouse_down) / sizeof(g_mouse_down[0]))) return false;
    return g_mouse_down[idx];
}

bool IsMouseButtonPressed(int button)
{
    int idx = button - MOUSE_BUTTON_LEFT;
    if (idx < 0 || idx >= (int)(sizeof(g_mouse_pressed) / sizeof(g_mouse_pressed[0]))) return false;
    return g_mouse_pressed[idx];
}

Vector2 GetMousePosition(void)
{
    return g_mouse_pos;
}

float GetMouseWheelMove(void)
{
    return g_mouse_wheel;
}
