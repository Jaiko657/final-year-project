#include "modules/core/toast.h"
#include "modules/systems/systems_registration.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static char  ui_toast_text[128] = {0};
static float ui_toast_timer     = 0.0f;

void ui_toast_init(void)
{
    ui_toast_timer = 0.0f;
    ui_toast_text[0] = '\0';
}

void ui_toast_update(float dt)
{
    if (ui_toast_timer > 0.0f) {
        ui_toast_timer -= dt;
        if (ui_toast_timer < 0.0f) ui_toast_timer = 0.0f;
    }
}

void ui_toast(float secs, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ui_toast_text, sizeof(ui_toast_text), fmt, ap);
    va_end(ap);
    ui_toast_timer = secs;
}

bool ecs_toast_is_active(void)
{
    return ui_toast_timer > 0.0f;
}

const char* ecs_toast_get_text(void)
{
    return ui_toast_text;
}

SYSTEMS_ADAPT_DT(sys_toast_update_adapt, ui_toast_update)
