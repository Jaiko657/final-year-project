#include "unity.h"

#include "modules/world/world_collision_internal.h"
#include "modules/world/world.h"

#include <stdlib.h>

static world_map_t make_min_map(int w, int h,
                                tiled_tileset_t* tilesets, size_t tileset_count,
                                tiled_layer_t* layers, size_t layer_count)
{
    world_map_t map = {0};
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

void test_world_resolve_rect_axis_px_resolves_x_without_changing_y(void)
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
    layer.gids[0] = 1u; // walkable
    layer.gids[1] = 2u; // solid

    tiled_layer_t layers[1] = { layer };
    world_map_t map = make_min_map(2, 1, tilesets, 1, layers, 1);
    TEST_ASSERT_TRUE_MESSAGE(world_collision_build_from_map(&map, "walls"), "world_collision_build_from_map failed");

    // Place an AABB so it penetrates a vertical wall on X while remaining within bounds on Y.
    float cx = (float)world_tile_size() - 1.0f;      // 31 => right edge at 35 (hx=4)
    float cy = (float)world_tile_size() - 0.6f;      // 31.4 => top at 31.9 (hy=0.5)
    float hx = 4.0f;
    float hy = 0.5f;

    TEST_ASSERT_FALSE_MESSAGE(world_is_walkable_rect_px(cx, cy, hx, hy), "expected starting rect to be non-walkable");
    TEST_ASSERT_TRUE_MESSAGE(world_resolve_rect_axis_px(&cx, &cy, hx, hy, true), "expected axis resolver to move rect");
    TEST_ASSERT_TRUE_MESSAGE(world_is_walkable_rect_px(cx, cy, hx, hy), "expected rect to be walkable after resolve");

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, (float)world_tile_size() - hx, cx);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, (float)world_tile_size() - 0.6f, cy);

    world_collision_shutdown();
    free(layer.gids);
}

void test_world_resolve_rect_axis_px_resolves_y_without_changing_x(void)
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
    layer.width = 1;
    layer.height = 2;
    layer.collision = true;
    layer.z_order = 0;
    layer.gids = (uint32_t*)calloc(2, sizeof(uint32_t));
    TEST_ASSERT_NOT_NULL(layer.gids);
    layer.gids[0] = 1u; // bottom walkable
    layer.gids[1] = 2u; // top solid

    tiled_layer_t layers[1] = { layer };
    world_map_t map = make_min_map(1, 2, tilesets, 1, layers, 1);
    TEST_ASSERT_TRUE_MESSAGE(world_collision_build_from_map(&map, "walls"), "world_collision_build_from_map failed");

    // Slightly penetrate the bottom of the top solid tile (a "ceiling" at y=32).
    // Axis-Y resolution should push down on Y without adding any lateral X shove.
    float hx = 6.0f;
    float hy = 2.0f;
    float cx0 = 16.0f;
    float cy0 = (float)world_tile_size() - 0.5f; // 31.5 => top=33.5 (penetrates y=32..)
    float cx = cx0;
    float cy = cy0;

    TEST_ASSERT_FALSE_MESSAGE(world_is_walkable_rect_px(cx, cy, hx, hy), "expected starting rect to be non-walkable");
    TEST_ASSERT_TRUE_MESSAGE(world_resolve_rect_axis_px(&cx, &cy, hx, hy, false), "expected axis resolver to move rect");
    TEST_ASSERT_TRUE_MESSAGE(world_is_walkable_rect_px(cx, cy, hx, hy), "expected rect to be walkable after resolve");

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, cx0, cx);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, (float)world_tile_size() - hy, cy);

    world_collision_shutdown();
    free(layer.gids);
}

void test_world_resolve_rect_mtv_px_resolves_ceiling_without_lateral_shove(void)
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
    layer.width = 1;
    layer.height = 2;
    layer.collision = true;
    layer.z_order = 0;
    layer.gids = (uint32_t*)calloc(2, sizeof(uint32_t));
    TEST_ASSERT_NOT_NULL(layer.gids);
    layer.gids[0] = 1u; // bottom walkable
    layer.gids[1] = 2u; // top solid

    tiled_layer_t layers[1] = { layer };
    world_map_t map = make_min_map(1, 2, tilesets, 1, layers, 1);
    TEST_ASSERT_TRUE_MESSAGE(world_collision_build_from_map(&map, "walls"), "world_collision_build_from_map failed");

    float hx = 6.0f;
    float hy = 2.0f;
    float cx0 = 16.0f;
    float cy0 = (float)world_tile_size() - 0.5f; // penetrates ceiling
    float cx = cx0;
    float cy = cy0;

    TEST_ASSERT_FALSE(world_is_walkable_rect_px(cx, cy, hx, hy));
    TEST_ASSERT_TRUE(world_resolve_rect_mtv_px(&cx, &cy, hx, hy));
    TEST_ASSERT_TRUE(world_is_walkable_rect_px(cx, cy, hx, hy));

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, cx0, cx);

    world_collision_shutdown();
    free(layer.gids);
}
