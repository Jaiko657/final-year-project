#include "unity.h"

#include "modules/world/world_collision_internal.h"

static tiled_tileset_t make_tileset(int first_gid, int tilecount, uint16_t* colliders, bool* no_merge)
{
    tiled_tileset_t ts = {0};
    ts.first_gid = first_gid;
    ts.tilecount = tilecount;
    ts.colliders = colliders;
    ts.no_merge_collider = no_merge;
    return ts;
}

void test_world_collision_decode_raw_gid_returns_mask_and_dynamic(void)
{
    // Put a single bit in the top-left subtile of a 4x4 mask (bit 0).
    uint16_t colliders[2] = {0};
    colliders[0] = (uint16_t)(1u << 0);

    bool no_merge[2] = {0};
    no_merge[0] = true;

    tiled_tileset_t tilesets[1];
    tilesets[0] = make_tileset(1, 2, colliders, no_merge);

    world_map_t map = {0};
    map.tileset_count = 1;
    map.tilesets = tilesets;

    uint16_t mask = 0;
    bool dynamic = false;
    TEST_ASSERT_TRUE(world_collision_decode_raw_gid(&map, 1u, &mask, &dynamic));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)(1u << 0), mask);
    TEST_ASSERT_TRUE(dynamic);
}

void test_world_collision_decode_raw_gid_applies_flip_h(void)
{
    // bit 0 (x=0,y=0) should flip horizontally to bit 3 (x=3,y=0).
    uint16_t colliders[1] = { (uint16_t)(1u << 0) };
    bool no_merge[1] = { false };

    tiled_tileset_t tilesets[1];
    tilesets[0] = make_tileset(1, 1, colliders, no_merge);

    world_map_t map = {0};
    map.tileset_count = 1;
    map.tilesets = tilesets;

    uint16_t mask = 0;
    bool dynamic = false;
    uint32_t raw = 1u | TILED_FLIPPED_HORIZONTALLY_FLAG;
    TEST_ASSERT_TRUE(world_collision_decode_raw_gid(&map, raw, &mask, &dynamic));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)(1u << 3), mask);
    TEST_ASSERT_FALSE(dynamic);
}

void test_world_collision_decode_raw_gid_applies_flip_v(void)
{
    // bit 0 (x=0,y=0) should flip vertically to bit 12 (x=0,y=3).
    uint16_t colliders[1] = { (uint16_t)(1u << 0) };
    bool no_merge[1] = { false };

    tiled_tileset_t tilesets[1];
    tilesets[0] = make_tileset(1, 1, colliders, no_merge);

    world_map_t map = {0};
    map.tileset_count = 1;
    map.tilesets = tilesets;

    uint16_t mask = 0;
    bool dynamic = false;
    uint32_t raw = 1u | TILED_FLIPPED_VERTICALLY_FLAG;
    TEST_ASSERT_TRUE(world_collision_decode_raw_gid(&map, raw, &mask, &dynamic));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)(1u << 12), mask);
    TEST_ASSERT_FALSE(dynamic);
}
