#pragma once
#include <chipmunk/chipmunk.h>
#include <stdbool.h>

typedef struct {
    int bodies;
    int shapes;
} world_physics_counts_t;

bool world_physics_ready(void);
void world_physics_init(void);
void world_physics_rebuild_static(void);
void world_physics_shutdown(void);

bool world_physics_add_body(cpBody* body);
bool world_physics_add_shape(cpShape* shape);
void world_physics_remove_body(cpBody* body);
void world_physics_remove_shape(cpShape* shape);

world_physics_counts_t world_physics_counts(void);
void world_physics_step(float dt);

// Debug helpers (read-only)
void world_physics_for_each_static_shape(void (*cb)(const cpBB* bb, void* ud), void* ud);
