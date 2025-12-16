#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "build_config.h"

/*
  REFERENCES:
  - https://gamedev.stackexchange.com/questions/174202/how-to-handle-player-input-with-fixed-rate-variable-fps-time-step
*/

/*
  Logical buttons the game understands.
  You can add more without touching systems that only read input_t.
*/
typedef enum {
    BTN_LEFT = 0,
    BTN_RIGHT,
    BTN_UP,
    BTN_DOWN,
    BTN_INTERACT,
    BTN_LIFT,
    BTN_MOUSE_L,
    BTN_MOUSE_R,
#if DEBUG_BUILD
    BTN_ASSET_DEBUG_PRINT,
    BTN_DEBUG_COLLIDER_ECS,
    BTN_DEBUG_COLLIDER_PHYSICS,
    BTN_DEBUG_COLLIDER_STATIC,
    BTN_DEBUG_TRIGGERS,
    BTN_DEBUG_INSPECT,
    BTN_DEBUG_FPS,
#endif
    BTN_COUNT               // <- Must be last as used to loop over enum until this point
} button_t;

/*
  Small 2D vector so the public header does not depend on Raylib.
*/
typedef struct { float x, y; } input_vec2;

/*
  Snapshot of input for systems to read.
  - 'down' is which buttons are held during the current frame.
  - 'pressed' captures rising edges (latched for the first fixed tick).
  - 'moveX/moveY' is a normalized vector derived from arrow/WASD buttons.
  - 'mouse' is the current mouse position (pixels).
  - 'mouse_wheel' is the scroll delta reported this frame.
*/
typedef struct {
    uint64_t   down;
    uint64_t   pressed;
    float      moveX, moveY;
    input_vec2 mouse;
    float      mouse_wheel;
} input_t;

/*
  Prepare a default control scheme (WASD + Arrows, E/Space, mouse buttons).
*/
void input_init_defaults(void);

/*
  Add a physical binding (Raylib key or mouse code) to a logical button.
*/
void input_bind(button_t btn, int keycode);

/*
  Poll OS input once per render frame and build an internal snapshot.
  Call this before stepping any number of fixed updates.
*/
void input_begin_frame(void);

/*
  Get the per-tick view of input for your fixed update.
  The first tick after input_begin_frame() sees 'pressed' edges and wheel delta;
  subsequent ticks for the same frame see pressed=0 and mouse_wheel=0.
*/
input_t input_for_tick(void);

/*
  Convenience checks for systems that want simple queries.
*/
static inline bool input_down(const input_t* in, button_t b)
{ return (in->down    & (1ull << b)) != 0; }

static inline bool input_pressed(const input_t* in, button_t b)
{ return (in->pressed & (1ull << b)) != 0; }
