#include "modules/core/camera.h"
#include "modules/systems/systems_registration.h"
#include "modules/core/logger.h"

#include <math.h>
#include <float.h>

typedef struct {
    camera_config_t config;
    v2f current;
    bool initialized;
} camera_state_t;

static camera_state_t g_camera;

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static v2f default_position(void) {
    return v2f_make(0.0f, 0.0f);
}

static rectf default_bounds(void) {
    return rectf_xywh(0.0f, 0.0f, 0.0f, 0.0f);
}

static void camera_reset_state(void) {
    g_camera.config = (camera_config_t){
        .target = ecs_null(),
        .position = default_position(),
        .offset = v2f_make(0.0f, 0.0f),
        .bounds = default_bounds(),
        .zoom = 1.0f,
        .padding = 64.0f,
        .deadzone_x = 16.0f,
        .deadzone_y = 16.0f,
    };
    g_camera.current = g_camera.config.position;
    g_camera.initialized = true;
}

void camera_init(void) {
    camera_reset_state();
}

void camera_shutdown(void) {
    camera_reset_state();
}

camera_config_t camera_get_config(void) {
    return g_camera.config;
}

void camera_set_config(const camera_config_t* cfg) {
    if (!cfg) return;
    g_camera.config = *cfg;
    if (g_camera.config.zoom <= 0.0f) g_camera.config.zoom = 1.0f;
    if (g_camera.config.padding < 0.0f) g_camera.config.padding = 0.0f;
    if (g_camera.config.deadzone_x < 0.0f) g_camera.config.deadzone_x = 0.0f;
    if (g_camera.config.deadzone_y < 0.0f) g_camera.config.deadzone_y = 0.0f;
    g_camera.current = g_camera.config.position;
    g_camera.initialized = true;
}

static v2f camera_target_position(void) {
    v2f desired = g_camera.config.position;
    v2f entity_pos;
    if (!ecs_get_position(g_camera.config.target, &entity_pos)) {
        return desired;
    }

    desired.x = entity_pos.x + g_camera.config.offset.x;
    desired.y = entity_pos.y + g_camera.config.offset.y;
    return desired;
}

void camera_tick(float dt) {
    (void)dt;
    if (!g_camera.initialized) {
        camera_reset_state();
    }

    v2f desired = camera_target_position();
    float dzx = g_camera.config.deadzone_x;
    float dzy = g_camera.config.deadzone_y;

    float dx = desired.x - g_camera.current.x;
    float dy = desired.y - g_camera.current.y;

    if (dx > dzx)       g_camera.current.x = desired.x - dzx;
    else if (dx < -dzx) g_camera.current.x = desired.x + dzx;
    if (dy > dzy)       g_camera.current.y = desired.y - dzy;
    else if (dy < -dzy) g_camera.current.y = desired.y + dzy;

    if (g_camera.config.bounds.w > 0.0f) {
        float min_x = g_camera.config.bounds.x;
        float max_x = g_camera.config.bounds.x + g_camera.config.bounds.w;
        g_camera.current.x = clampf(g_camera.current.x, min_x, max_x);
    }
    if (g_camera.config.bounds.h > 0.0f) {
        float min_y = g_camera.config.bounds.y;
        float max_y = g_camera.config.bounds.y + g_camera.config.bounds.h;
        g_camera.current.y = clampf(g_camera.current.y, min_y, max_y);
    }
}

camera_view_t camera_get_view(void) {
    camera_view_t view = {
        .center = g_camera.current,
        .zoom = g_camera.config.zoom > 0.0f ? g_camera.config.zoom : 1.0f,
        .padding = g_camera.config.padding >= 0.0f ? g_camera.config.padding : 0.0f,
    };
    return view;
}

SYSTEMS_ADAPT_DT(sys_camera_tick_adapt, camera_tick)
