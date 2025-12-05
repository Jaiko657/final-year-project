#pragma once
#include <chipmunk/chipmunk.h>

typedef enum {
    PHYS_NONE = 0,
    PHYS_DYNAMIC,
    PHYS_KINEMATIC,
    PHYS_STATIC
} PhysicsType;

typedef struct {
    PhysicsType type;
    float mass;
    float inv_mass;
    float restitution;
    float friction;

    // Collision filtering (defaults to 0 == Chipmunk defaults)
    unsigned int category_bits;
    unsigned int mask_bits;

    // Chipmunk runtime handles
    cpBody*  cp_body;
    cpShape* cp_shape;
} cmp_phys_body_t;

// Internal helpers (component indices)
void ecs_phys_body_create_for_entity(int idx);
void ecs_phys_body_destroy_for_entity(int idx);
void ecs_phys_destroy_all(void);
