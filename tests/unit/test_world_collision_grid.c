#include "unity.h"

#include "world_collision.h"
#include "world.h"

#include <stdlib.h>
#include <string.h>

static tiled_map_t make_min_map(int w, int h,
                                tiled_tileset_t* tilesets, size_t tileset_count,
                                tiled_layer_t* layers, size_t layer_count)
{
    tiled_map_t map = {0};
    map.width = w;
    map.height = h;
    map.tilewidth = world_tile_size();
    map.tileheight = world_tile_size();
    map.tilesets = tilesets;
    map.tileset_count = tileset_count;
    map.layers = layers;
    map.layer_count = layer_count;
    return map;
}

void test_world_collision_build_from_map_populates_queries(void)
{
    const uint16_t FULL = (uint16_t)((1u << 16) - 1u);

    uint16_t colliders[2] = {0, FULL};
    bool no_merge[2] = {false, true};
    tiled_tileset_t tilesets[1] = {0};
    tilesets[0].first_gid = 1;
    tilesets[0].tilecount = 2;
    tilesets[0].colliders = colliders;
    tilesets[0].no_merge_collider = no_merge;

    tiled_layer_t layer = {0};
    layer.name = "walls";
    layer.width = 2;
    layer.height = 1;
    layer.collision = false;
    layer.z_order = 0;
    layer.gids = (uint32_t*)calloc(2, sizeof(uint32_t));
    TEST_ASSERT_NOT_NULL(layer.gids);
    layer.gids[0] = 1u; // local 0 => mask 0 => walkable
    layer.gids[1] = 2u; // local 1 => full mask => solid + dynamic flag true

    tiled_layer_t layers[1] = { layer };
    tiled_map_t map = make_min_map(2, 1, tilesets, 1, layers, 1);

    TEST_ASSERT_TRUE(world_collision_build_from_map(&map, "walls"));

    int tw = 0, th = 0;
    world_size_tiles(&tw, &th);
    TEST_ASSERT_EQUAL_INT(2, tw);
    TEST_ASSERT_EQUAL_INT(1, th);

    TEST_ASSERT_EQUAL_INT(WORLD_TILE_WALKABLE, world_tile_at(0, 0));
    TEST_ASSERT_EQUAL_INT(WORLD_TILE_SOLID, world_tile_at(1, 0));
    TEST_ASSERT_EQUAL_UINT16(0u, world_subtile_mask_at(0, 0));
    TEST_ASSERT_EQUAL_UINT16(FULL, world_subtile_mask_at(1, 0));

    TEST_ASSERT_FALSE(world_tile_is_dynamic(0, 0));
    TEST_ASSERT_TRUE(world_tile_is_dynamic(1, 0));

    TEST_ASSERT_TRUE(world_is_walkable_px(4.0f, 4.0f));
    TEST_ASSERT_FALSE(world_is_walkable_px((float)world_tile_size() + 4.0f, 4.0f));

    v2f spawn = world_get_spawn_px();
    TEST_ASSERT_TRUE(spawn.x > 0.0f);
    TEST_ASSERT_TRUE(spawn.y > 0.0f);

    world_collision_shutdown();
    free(layer.gids);
}

void test_world_collision_refresh_tile_updates_mask_and_dynamic(void)
{
    const uint16_t FULL = (uint16_t)((1u << 16) - 1u);

    uint16_t colliders[2] = {0, FULL};
    bool no_merge[2] = {false, true};
    tiled_tileset_t tilesets[1] = {0};
    tilesets[0].first_gid = 1;
    tilesets[0].tilecount = 2;
    tilesets[0].colliders = colliders;
    tilesets[0].no_merge_collider = no_merge;

    tiled_layer_t layer = {0};
    layer.name = "walls";
    layer.width = 2;
    layer.height = 1;
    layer.collision = true;
    layer.z_order = 0;
    layer.gids = (uint32_t*)calloc(2, sizeof(uint32_t));
    TEST_ASSERT_NOT_NULL(layer.gids);
    layer.gids[0] = 1u;
    layer.gids[1] = 2u;

    tiled_layer_t layers[1] = { layer };
    tiled_map_t map = make_min_map(2, 1, tilesets, 1, layers, 1);

    TEST_ASSERT_TRUE(world_collision_build_from_map(&map, "walls"));
    TEST_ASSERT_EQUAL_INT(WORLD_TILE_SOLID, world_tile_at(1, 0));
    TEST_ASSERT_TRUE(world_tile_is_dynamic(1, 0));

    // Change the tile to local 0 (walkable, not dynamic) and refresh.
    layer.gids[1] = 1u;
    world_collision_refresh_tile(&map, 1, 0);
    TEST_ASSERT_EQUAL_INT(WORLD_TILE_WALKABLE, world_tile_at(1, 0));
    TEST_ASSERT_FALSE(world_tile_is_dynamic(1, 0));

    // Tile rect checks
    TEST_ASSERT_TRUE(world_is_walkable_rect_px((float)world_tile_size() + 4.0f, 4.0f, 1.0f, 1.0f));

    world_collision_shutdown();
    free(layer.gids);
}

void test_world_collision_line_of_sight_blocked_by_solid(void)
{
    const uint16_t FULL = (uint16_t)((1u << 16) - 1u);

    uint16_t colliders[2] = {0, FULL};
    bool no_merge[2] = {false, false};
    tiled_tileset_t tilesets[1] = {0};
    tilesets[0].first_gid = 1;
    tilesets[0].tilecount = 2;
    tilesets[0].colliders = colliders;
    tilesets[0].no_merge_collider = no_merge;

    tiled_layer_t layer = {0};
    layer.name = "walls";
    layer.width = 2;
    layer.height = 1;
    layer.collision = true;
    layer.z_order = 0;
    layer.gids = (uint32_t*)calloc(2, sizeof(uint32_t));
    TEST_ASSERT_NOT_NULL(layer.gids);
    layer.gids[0] = 1u;
    layer.gids[1] = 2u; // solid

    tiled_layer_t layers[1] = { layer };
    tiled_map_t map = make_min_map(2, 1, tilesets, 1, layers, 1);

    TEST_ASSERT_TRUE(world_collision_build_from_map(&map, "walls"));

    // Ray from tile 0 into tile 1 should be blocked (hx/hy=0 to test point walkability).
    float x0 = 4.0f, y0 = 4.0f;
    float x1 = (float)world_tile_size() + 4.0f, y1 = 4.0f;
    TEST_ASSERT_FALSE(world_has_line_of_sight(x0, y0, x1, y1, -1.0f, 0.0f, 0.0f));

    world_collision_shutdown();
    free(layer.gids);
}

void test_world_collision_line_of_sight_clear_returns_true(void)
{
    uint16_t colliders[1] = {0};
    bool no_merge[1] = {false};
    tiled_tileset_t tilesets[1] = {0};
    tilesets[0].first_gid = 1;
    tilesets[0].tilecount = 1;
    tilesets[0].colliders = colliders;
    tilesets[0].no_merge_collider = no_merge;

    tiled_layer_t layer = {0};
    layer.name = "walls";
    layer.width = 2;
    layer.height = 1;
    layer.collision = true;
    layer.z_order = 0;
    layer.gids = (uint32_t*)calloc(2, sizeof(uint32_t));
    TEST_ASSERT_NOT_NULL(layer.gids);
    layer.gids[0] = 1u;
    layer.gids[1] = 1u;

    tiled_layer_t layers[1] = { layer };
    tiled_map_t map = make_min_map(2, 1, tilesets, 1, layers, 1);
    TEST_ASSERT_TRUE(world_collision_build_from_map(&map, "walls"));

    float x0 = 4.0f, y0 = 4.0f;
    float x1 = (float)world_tile_size() + 4.0f, y1 = 4.0f;
    TEST_ASSERT_TRUE(world_has_line_of_sight(x0, y0, x1, y1, -1.0f, 0.0f, 0.0f));

    int px = 0, py = 0;
    world_size_px(&px, &py);
    TEST_ASSERT_EQUAL_INT(2 * world_tile_size(), px);
    TEST_ASSERT_EQUAL_INT(1 * world_tile_size(), py);

    TEST_ASSERT_EQUAL_INT(WORLD_TILE_VOID, world_tile_at(-1, 0));
    TEST_ASSERT_EQUAL_UINT16(0u, world_subtile_mask_at(123, 456));
    TEST_ASSERT_FALSE(world_tile_is_dynamic(-1, -1));

    world_collision_shutdown();
    v2f spawn = world_get_spawn_px();
    TEST_ASSERT_EQUAL_FLOAT(0.0f, spawn.x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, spawn.y);

    free(layer.gids);
}

void test_world_collision_decode_raw_gid_fails_without_matching_tileset(void)
{
    uint16_t colliders[1] = {0};
    bool no_merge[1] = {false};
    tiled_tileset_t tilesets[1] = {0};
    tilesets[0].first_gid = 10;
    tilesets[0].tilecount = 1;
    tilesets[0].colliders = colliders;
    tilesets[0].no_merge_collider = no_merge;

    tiled_map_t map = {0};
    map.tileset_count = 1;
    map.tilesets = tilesets;

    uint16_t mask = 123;
    bool dynamic = true;
    TEST_ASSERT_FALSE(world_collision_decode_raw_gid(&map, 1u, &mask, &dynamic));
}

void test_world_collision_build_from_map_ignores_non_collision_layers(void)
{
    // Cover the "not a collision layer" path: layer has no collision flag and name doesn't match.
    uint16_t colliders[1] = { (uint16_t)((1u << 16) - 1u) };
    bool no_merge[1] = {false};
    tiled_tileset_t tilesets[1] = {0};
    tilesets[0].first_gid = 1;
    tilesets[0].tilecount = 1;
    tilesets[0].colliders = colliders;
    tilesets[0].no_merge_collider = no_merge;

    tiled_layer_t layer = {0};
    layer.name = "not_walls";
    layer.width = 1;
    layer.height = 1;
    layer.collision = false;
    layer.gids = (uint32_t*)calloc(1, sizeof(uint32_t));
    TEST_ASSERT_NOT_NULL(layer.gids);
    layer.gids[0] = 1u;

    tiled_layer_t layers[1] = { layer };
    tiled_map_t map = make_min_map(1, 1, tilesets, 1, layers, 1);
    map.tilewidth = 16;
    map.tileheight = 16;

    TEST_ASSERT_TRUE(world_collision_build_from_map(&map, "walls"));
    TEST_ASSERT_EQUAL_INT(WORLD_TILE_WALKABLE, world_tile_at(0, 0));

    world_collision_shutdown();
    free(layer.gids);
}
