#include "unity.h"

#include "world_map.h"
#include "world_query.h"

#include <stdlib.h>

static void ensure_clean_world(void)
{
    // Idempotent, and ensures tests don't depend on ordering.
    world_shutdown();
}

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

static tiled_map_t make_map_1x1_two_layers(uint16_t mask_for_tile1, uint32_t layer0_gid, uint32_t layer1_gid, bool layer1_collision)
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

    map.layer_count = 2;
    map.layers = (tiled_layer_t*)calloc(2, sizeof(tiled_layer_t));

    map.layers[0].collision = true;
    map.layers[0].width = 1;
    map.layers[0].height = 1;
    map.layers[0].gids = (uint32_t*)calloc(1, sizeof(uint32_t));
    map.layers[0].gids[0] = layer0_gid;

    map.layers[1].collision = layer1_collision;
    map.layers[1].width = 1;
    map.layers[1].height = 1;
    map.layers[1].gids = (uint32_t*)calloc(1, sizeof(uint32_t));
    map.layers[1].gids[0] = layer1_gid;

    return map;
}

void test_world_map_tile_edits_are_queued_until_apply(void)
{
    ensure_clean_world();

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

void test_world_map_generation_increments_and_get_map_tracks_lifecycle(void)
{
    ensure_clean_world();

    uint32_t g0 = world_map_generation();
    TEST_ASSERT_NULL(world_get_tiled_map());

    tiled_map_t map1 = make_map_1x1(0u);
    TEST_ASSERT_TRUE(world_test_set_map(&map1, NULL));
    uint32_t g1 = world_map_generation();
    TEST_ASSERT_EQUAL_UINT32(g0 + 1u, g1);
    TEST_ASSERT_NOT_NULL(world_get_tiled_map());

    tiled_map_t map2 = make_map_1x1(0u);
    TEST_ASSERT_TRUE(world_test_set_map(&map2, NULL));
    uint32_t g2 = world_map_generation();
    TEST_ASSERT_EQUAL_UINT32(g1 + 1u, g2);

    world_shutdown();
    TEST_ASSERT_NULL(world_get_tiled_map());
}

void test_world_set_tile_gid_rejects_invalid_inputs(void)
{
    ensure_clean_world();

    const uint16_t FULL = (uint16_t)((1u << 16) - 1u);

    // Layer 0 is collision (has gids); layer 1 is non-collision and intentionally has no gids.
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
    map.tilesets[0].colliders[0] = FULL;
    map.tilesets[0].no_merge_collider = (bool*)calloc(1, sizeof(bool));

    map.layer_count = 2;
    map.layers = (tiled_layer_t*)calloc(2, sizeof(tiled_layer_t));
    map.layers[0].collision = true;
    map.layers[0].width = 1;
    map.layers[0].height = 1;
    map.layers[0].gids = (uint32_t*)calloc(1, sizeof(uint32_t));
    map.layers[0].gids[0] = 1u;

    map.layers[1].collision = false;
    map.layers[1].width = 1;
    map.layers[1].height = 1;
    map.layers[1].gids = NULL;

    TEST_ASSERT_TRUE(world_test_set_map(&map, NULL));

    TEST_ASSERT_FALSE(world_set_tile_gid(-1, 0, 0, 0u));
    TEST_ASSERT_FALSE(world_set_tile_gid(999, 0, 0, 0u));
    TEST_ASSERT_FALSE(world_set_tile_gid(0, -1, 0, 0u));
    TEST_ASSERT_FALSE(world_set_tile_gid(0, 0, -1, 0u));
    TEST_ASSERT_FALSE(world_set_tile_gid(0, 1, 0, 0u));
    TEST_ASSERT_FALSE(world_set_tile_gid(0, 0, 1, 0u));
    TEST_ASSERT_FALSE(world_set_tile_gid(1, 0, 0, 0u)); // no gids on layer 1

    world_shutdown();

    // Not ready => reject edits.
    TEST_ASSERT_FALSE(world_set_tile_gid(0, 0, 0, 0u));
}

void test_world_apply_tile_edits_refreshes_collision_only_for_collision_layers(void)
{
    ensure_clean_world();

    const uint16_t FULL = (uint16_t)((1u << 16) - 1u);
    tiled_map_t map = make_map_1x1_two_layers(FULL, 1u, 1u, false);

    TEST_ASSERT_TRUE(world_test_set_map(&map, NULL));
    TEST_ASSERT_EQUAL_INT(WORLD_TILE_SOLID, world_tile_at(0, 0));

    // Editing a non-collision layer should not affect the collision grid.
    TEST_ASSERT_TRUE(world_set_tile_gid(1, 0, 0, 0u));
    world_apply_tile_edits();
    TEST_ASSERT_EQUAL_INT(WORLD_TILE_SOLID, world_tile_at(0, 0));
    const tiled_map_t* m = world_get_tiled_map();
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_UINT32(0u, m->layers[1].gids[0]);

    // Editing the collision layer should refresh collision.
    TEST_ASSERT_TRUE(world_set_tile_gid(0, 0, 0, 0u));
    world_apply_tile_edits();
    TEST_ASSERT_EQUAL_INT(WORLD_TILE_WALKABLE, world_tile_at(0, 0));

    world_shutdown();
}

void test_world_apply_tile_edits_is_safe_when_unloaded(void)
{
    ensure_clean_world();
    world_apply_tile_edits();
    world_shutdown();
    world_apply_tile_edits();
}

void test_world_load_from_tmx_fails_for_missing_file_and_preserves_state(void)
{
    ensure_clean_world();
    uint32_t g0 = world_map_generation();

    TEST_ASSERT_FALSE(world_load_from_tmx("build/testdata/does_not_exist.tmx", NULL));
    TEST_ASSERT_EQUAL_UINT32(g0, world_map_generation());
    TEST_ASSERT_NULL(world_get_tiled_map());
}
