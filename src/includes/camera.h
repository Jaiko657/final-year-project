#pragma once
#include "engine_types.h"
#include "ecs.h"

// Public configuration used to control the camera module.
typedef struct {
    ecs_entity_t target; // Entity to follow. ecs_null() disables following.
    v2f position;        // Manual center used when no target is available.
    v2f offset;          // Offset from the target's position.
    rectf bounds;        // Optional bounds for the camera center (w/h <= 0 disables).
    float stiffness;     // Follow stiffness; <= 0 means snap to target immediately.
    float zoom;          // Render zoom. 1.0 = default scale.
} camera_config_t;

// Render-time view information computed by the module.
typedef struct {
    v2f center;
    float zoom;
} camera_view_t;

void camera_init(void);
void camera_shutdown(void);
void camera_tick(float dt);

camera_config_t camera_get_config(void);
void camera_set_config(const camera_config_t* cfg);
camera_view_t camera_get_view(void);
