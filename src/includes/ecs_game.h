#pragma once
#include "ecs.h"

int init_entities(int W, int H);

typedef enum { ITEM_COIN = 1, ITEM_HAT = 2 } item_kind_t;

// Component adders (game-specific)
void cmp_add_inventory(ecs_entity_t e);
void cmp_add_item     (ecs_entity_t e, item_kind_t k);
void cmp_add_vendor   (ecs_entity_t e, item_kind_t sells, int price);

// Game HUD helpers
void game_get_player_stats(int* outCoins, bool* outHasHat);
bool game_vendor_hint_is_active(int* out_x, int* out_y, const char** out_text);

// Needs called outside of ecs to avoid dependency (currently called in main after ecs_init)
void ecs_register_game_systems(void);
