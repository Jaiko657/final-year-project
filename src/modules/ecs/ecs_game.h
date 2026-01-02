#pragma once
#include "modules/ecs/ecs.h"

bool init_entities(const char* tmx_path);

// Component adders (game-specific)
void cmp_add_plastic  (ecs_entity_t e);
void cmp_add_storage  (ecs_entity_t e, int capacity);

// Game HUD helpers
bool game_get_tardas_storage(int* out_plastic, int* out_capacity);

// Inspect/debug helpers (read-only access)
bool ecs_game_get_storage(ecs_entity_t e, int* out_plastic, int* out_capacity);

// Needs called outside of ecs to avoid dependency (currently called in main after ecs_init)
void ecs_register_game_systems(void);
