#include "modules/core/input.h"

void input_init_defaults(void) { }
void input_bind(button_t btn, int keycode) { (void)btn; (void)keycode; }

void input_begin_frame(void) { }

input_t input_for_tick(void)
{
    return (input_t){0};
}

