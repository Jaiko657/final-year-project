#include "unity.h"

#include "world_map.h"
#include "world_query.h"

#include <stdlib.h>

static tiled_map_t make_map_1x1(uint16_t mask_for_tile1)
{
    tiled_map_t map = {0};
    map.width = 1;
    map.height = 1;
    map.tilewidth = world_tile_size();
    map.tileheight = world_tile_size();

    map.tileset_count = 1;
    map.tilesets = (tiled_tileset_t*)calloc(1, sizeof(tiled_tileset_t));
    map.tilesets[0].first_gid = 1;
    map.tilesets[0].tilecount = 1;
    map.tilesets[0].colliders = (uint16_t*)calloc(1, sizeof(uint16_t));
    map.tilesets[0].colliders[0] = mask_for_tile1;
    map.tilesets[0].no_merge_collider = (bool*)calloc(1, sizeof(bool));

    map.layer_count = 1;
    map.layers = (tiled_layer_t*)calloc(1, sizeof(tiled_layer_t));
    map.layers[0].collision = true;
    map.layers[0].width = 1;
    map.layers[0].height = 1;
    map.layers[0].gids = (uint32_t*)calloc(1, sizeof(uint32_t));
    map.layers[0].gids[0] = 1u;

    return map;
}

void test_world_map_tile_edits_are_queued_until_apply(void)
{
    const uint16_t FULL = (uint16_t)((1u << 16) - 1u);
    tiled_map_t map = make_map_1x1(FULL);

    TEST_ASSERT_TRUE(world_test_set_map(&map, NULL));
    // world_test_set_map takes ownership

    TEST_ASSERT_EQUAL_INT(WORLD_TILE_SOLID, world_tile_at(0, 0));

    // Queue making tile empty (gid 0 => no collider).
    TEST_ASSERT_TRUE(world_set_tile_gid(0, 0, 0, 0u));

    // Still solid before apply.
    TEST_ASSERT_EQUAL_INT(WORLD_TILE_SOLID, world_tile_at(0, 0));

    world_apply_tile_edits();

    // Now walkable after apply.
    TEST_ASSERT_EQUAL_INT(WORLD_TILE_WALKABLE, world_tile_at(0, 0));

    world_shutdown();
}

