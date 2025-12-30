#pragma once
#include "modules/ecs/ecs.h"

bool init_entities(const char* tmx_path);

typedef enum { ITEM_COIN = 1, ITEM_HAT = 2 } item_kind_t;

// Component adders (game-specific)
void cmp_add_inventory(ecs_entity_t e);
void cmp_add_item     (ecs_entity_t e, item_kind_t k);
void cmp_add_vendor   (ecs_entity_t e, item_kind_t sells, int price);

// Game HUD helpers
void game_get_player_stats(int* outCoins, bool* outHasHat);
bool game_vendor_hint_is_active(int* out_x, int* out_y, const char** out_text);

// Inspect/debug helpers (read-only access)
bool ecs_game_get_item(ecs_entity_t e, int* out_kind);
bool ecs_game_get_inventory(ecs_entity_t e, int* out_coins, bool* out_has_hat);
bool ecs_game_get_vendor(ecs_entity_t e, int* out_sells, int* out_price);

// Needs called outside of ecs to avoid dependency (currently called in main after ecs_init)
void ecs_register_game_systems(void);
