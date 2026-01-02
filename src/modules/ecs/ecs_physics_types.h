#pragma once

#include <stdbool.h>
#include <stdint.h>

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

    // Collision filtering (0 means "default"/allow-all).
    unsigned int category_bits;
    unsigned int mask_bits;

    // Runtime flag: becomes true once the entity has the required components (POS+COL+PHYS_BODY)
    // and is participating in the physics-lite step.
    bool created;
} cmp_phys_body_t;

enum {
    PHYS_CAT_PLAYER = 1u << 0
};
