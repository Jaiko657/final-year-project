#pragma once
#include <stdbool.h>

#define KEY_NULL 0
#define KEY_KB_MENU 500

#define KEY_LEFT 1
#define KEY_RIGHT 2
#define KEY_UP 3
#define KEY_DOWN 4
#define KEY_A 5
#define KEY_D 6
#define KEY_W 7
#define KEY_S 8
#define KEY_E 9
#define KEY_C 10
#define KEY_J 11
#define KEY_SPACE 12
#define KEY_ONE 13
#define KEY_TWO 14
#define KEY_THREE 15
#define KEY_FOUR 16
#define KEY_FIVE 17
#define KEY_R 18
#define KEY_GRAVE 19

#define MOUSE_BUTTON_LEFT 1000
#define MOUSE_BUTTON_RIGHT 1001
#define MOUSE_BUTTON_BACK 1007

#define MOUSE_LEFT_BUTTON MOUSE_BUTTON_LEFT
#define MOUSE_RIGHT_BUTTON MOUSE_BUTTON_RIGHT

typedef struct Vector2 { float x; float y; } Vector2;

bool IsKeyDown(int key);
bool IsKeyPressed(int key);
bool IsMouseButtonDown(int button);
bool IsMouseButtonPressed(int button);
Vector2 GetMousePosition(void);
float GetMouseWheelMove(void);

void raylib_stub_reset(void);
void raylib_stub_set_key_down(int key, bool down);
void raylib_stub_set_key_pressed(int key, bool pressed);
void raylib_stub_set_mouse_down(int button, bool down);
void raylib_stub_set_mouse_pressed(int button, bool pressed);
void raylib_stub_set_mouse_pos(float x, float y);
void raylib_stub_set_mouse_wheel(float delta);
