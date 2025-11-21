#pragma once
#include "engine_types.h"
#include "ecs.h"

typedef struct {
    ecs_entity_t target;
    v2f position;
    v2f offset;
    rectf bounds;
    float stiffness;
    float zoom;
} camera_config_t;

typedef struct {
    v2f center;
    float zoom;
} camera_view_t;

void camera_init(void);
void camera_shutdown(void);
camera_config_t camera_get_config(void);
void camera_set_config(const camera_config_t* cfg);
void camera_tick(float dt);
camera_view_t camera_get_view(void);
