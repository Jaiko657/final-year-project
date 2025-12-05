#include "../includes/input.h"
#include "raylib.h"
#include <string.h>
#include <math.h>

#define MAX_KEYS_PER_BTN 6

/*
  Internal storage for button bindings:
  Each logical button can map to up to MAX_KEYS_PER_BTN physical codes.
*/
typedef struct {
    int keys[MAX_KEYS_PER_BTN];
    int key_count;
} button_binding_t;

/*
  Static state that persists across frames.
*/
static button_binding_t g_bindings[BTN_COUNT];
static uint64_t s_prev_down_bits = 0;
static input_t  s_frame_input;
static bool     s_edges_available = false;
static uint64_t s_latched_pressed = 0;
static float    s_latched_wheel   = 0.0f;

/*
  Small helper macro for building bit masks for buttons.
*/
#define BUTTON_BIT(b) (1ull << (b))

/*
  Add a physical code to a logical button if there is space.
*/
static void bind_add(button_t b, int code){
    if ((unsigned)b >= BTN_COUNT) return;
    if (g_bindings[b].key_count < MAX_KEYS_PER_BTN) {
        g_bindings[b].keys[g_bindings[b].key_count++] = code;
    }
}

/*
  Initialize clean state and install a sensible default layout.
*/
void input_init_defaults(void){
    memset(g_bindings, 0, sizeof(g_bindings));
    s_prev_down_bits   = 0;
    s_edges_available  = false;

    /* Movement: Arrows + WASD */
    bind_add(BTN_LEFT,  KEY_LEFT);   bind_add(BTN_LEFT,  KEY_A);
    bind_add(BTN_RIGHT, KEY_RIGHT);  bind_add(BTN_RIGHT, KEY_D);
    bind_add(BTN_UP,    KEY_UP);     bind_add(BTN_UP,    KEY_W);
    bind_add(BTN_DOWN,  KEY_DOWN);   bind_add(BTN_DOWN,  KEY_S);

    /* Interact: E */
    bind_add(BTN_INTERACT, KEY_E);

    /* Mouse buttons */
    bind_add(BTN_MOUSE_L, MOUSE_LEFT_BUTTON);
    bind_add(BTN_MOUSE_R, MOUSE_RIGHT_BUTTON);

    bind_add(BTN_ASSET_DEBUG_PRINT, KEY_SPACE);

    /* Debug collider modes */
    bind_add(BTN_DEBUG_COLLIDER_0, KEY_ZERO);
    bind_add(BTN_DEBUG_COLLIDER_1, KEY_ONE);
    bind_add(BTN_DEBUG_COLLIDER_2, KEY_TWO);
    bind_add(BTN_DEBUG_COLLIDER_3, KEY_THREE);
}

/*
  Public binding API: attach a Raylib key/mouse code to a logical button.
*/
void input_bind(button_t btn, int keycode){
    bind_add(btn, keycode);
}

/*
  Physical polling helpers:
  Raylib uses disjoint ranges for keyboard vs mouse.
  We detect which family the code belongs to and query the proper function.
*/
static bool phys_is_down(int code){
    if (code >= KEY_NULL && code <= KEY_KB_MENU) return IsKeyDown(code);
    return IsMouseButtonDown(code);
}

static bool phys_is_pressed(int code){
    if (code >= KEY_NULL && code <= KEY_KB_MENU) return IsKeyPressed(code);
    return IsMouseButtonPressed(code);
}

/*
  Poll the OS once per render frame and compute:
  - 'down' bitset for buttons that are held
  - 'pressed' bitset for rising edges (including Raylib's own edge helpers)
  Also capture mouse position and wheel delta, and derive a normalized move axis.
*/
void input_begin_frame(void){
    input_t in;
    memset(&in, 0, sizeof(in));

    uint64_t down_bits = 0;
    for (int b = 0; b < BTN_COUNT; ++b){
        for (int i = 0; i < g_bindings[b].key_count; ++i){
            if (phys_is_down(g_bindings[b].keys[i])) { down_bits |= BUTTON_BIT(b); break; }
        }
    }

    // edges seen THIS render frame
    uint64_t pressed_now = down_bits & ~s_prev_down_bits;
    for (int b = 0; b < BTN_COUNT; ++b){
        bool any_pressed = false;
        for (int i = 0; i < g_bindings[b].key_count; ++i){
            if (phys_is_pressed(g_bindings[b].keys[i])) { any_pressed = true; break; }
        }
        if (any_pressed) pressed_now |= BUTTON_BIT(b);
    }

    // LATCH edges + wheel until a fixed tick consumes them
    s_latched_pressed |= pressed_now;
    s_latched_wheel   += GetMouseWheelMove();

    s_prev_down_bits = down_bits;

    Vector2 m = GetMousePosition();
    in.down    = down_bits;
    in.pressed = s_latched_pressed;   // <- latched
    in.mouse.x = m.x;
    in.mouse.y = m.y;
    in.mouse_wheel = s_latched_wheel; // <- latched

    in.moveX = ((down_bits & BUTTON_BIT(BTN_RIGHT)) ? 1.f : 0.f)
             - ((down_bits & BUTTON_BIT(BTN_LEFT )) ? 1.f : 0.f);
    in.moveY = ((down_bits & BUTTON_BIT(BTN_DOWN )) ? 1.f : 0.f)
             - ((down_bits & BUTTON_BIT(BTN_UP   )) ? 1.f : 0.f);
    float mag = sqrtf(in.moveX*in.moveX + in.moveY*in.moveY);
    if (mag > 0.f){ in.moveX /= mag; in.moveY /= mag; }

    s_frame_input     = in;
    s_edges_available = true;
}

/*
  Provide the latched snapshot to a fixed tick.
  Only the first tick after input_begin_frame() gets 'pressed' edges and wheel delta;
  following ticks for the same frame get those cleared.
*/
input_t input_for_tick(void){
    input_t out = s_frame_input;
    if (!s_edges_available){
        out.pressed     = 0;
        out.mouse_wheel = 0;
    }
    // First tick after begin_frame consumes the latched edges/wheel
    s_edges_available = false;
    s_latched_pressed = 0;
    s_latched_wheel   = 0.0f;
    return out;
}
