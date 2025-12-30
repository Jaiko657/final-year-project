#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs_physics.h"

void ecs_register_physics_component_hooks(void)
{
    ecs_register_component_destroy_hook(ENUM_PHYS_BODY, ecs_phys_body_destroy_for_entity);
    ecs_register_phys_body_create_hook(ecs_phys_body_create_for_entity);
}
