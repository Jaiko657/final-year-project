#include "unity.h"

#include "modules/world/world.h"
#include "modules/world/world_renderer.h"
#include "modules/world/world_query.h"
#include "test_log_sink.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static void ensure_clean_world(void)
{
    // Idempotent, and ensures tests don't depend on ordering.
    world_shutdown();
}

static bool ensure_dir(const char* path)
{
    if (mkdir(path, 0755) == 0) return true;
    return errno == EEXIST;
}

static bool write_text_file(const char* path, const char* contents)
{
    FILE* f = fopen(path, "w");
    if (!f) return false;
    fputs(contents, f);
    fclose(f);
    return true;
}

static bool write_test_tileset(const char* path, const char* collider_value)
{
    char buf[512];
    snprintf(buf, sizeof(buf),
        "<tileset name=\"test\" tilewidth=\"%d\" tileheight=\"%d\" tilecount=\"1\" columns=\"1\">"
        "<image source=\"tiles.png\" width=\"%d\" height=\"%d\"/>"
        "<tile id=\"0\"><properties>"
        "<property name=\"collider\" value=\"%s\"/>"
        "</properties></tile>"
        "</tileset>",
        world_tile_size(), world_tile_size(),
        world_tile_size(), world_tile_size(),
        collider_value ? collider_value : "[0000],[0000],[0000],[0000]"
    );
    return write_text_file(path, buf);
}

static bool write_test_map_one_layer(const char* path, const char* tsx_rel, uint32_t gid, bool collision)
{
    char buf[512];
    snprintf(buf, sizeof(buf),
        "<map width=\"1\" height=\"1\" tilewidth=\"%d\" tileheight=\"%d\">"
        "<tileset firstgid=\"1\" source=\"%s\"/>"
        "<layer name=\"L0\" width=\"1\" height=\"1\">"
        "<properties><property name=\"collision\" value=\"%s\"/></properties>"
        "<data>%u</data></layer>"
        "</map>",
        world_tile_size(), world_tile_size(),
        tsx_rel ? tsx_rel : "tiles.tsx",
        collision ? "true" : "false",
        (unsigned)gid
    );
    return write_text_file(path, buf);
}

static bool write_test_map_two_layers(const char* path, const char* tsx_rel, uint32_t layer0_gid, uint32_t layer1_gid, bool layer1_collision)
{
    char buf[768];
    snprintf(buf, sizeof(buf),
        "<map width=\"1\" height=\"1\" tilewidth=\"%d\" tileheight=\"%d\">"
        "<tileset firstgid=\"1\" source=\"%s\"/>"
        "<layer name=\"L0\" width=\"1\" height=\"1\">"
        "<properties><property name=\"collision\" value=\"true\"/></properties>"
        "<data>%u</data></layer>"
        "<layer name=\"L1\" width=\"1\" height=\"1\">"
        "<properties><property name=\"collision\" value=\"%s\"/></properties>"
        "<data>%u</data></layer>"
        "</map>",
        world_tile_size(), world_tile_size(),
        tsx_rel ? tsx_rel : "tiles.tsx",
        (unsigned)layer0_gid,
        layer1_collision ? "true" : "false",
        (unsigned)layer1_gid
    );
    return write_text_file(path, buf);
}

static bool write_world_testdata(bool two_layers, uint32_t layer0_gid, uint32_t layer1_gid, bool layer1_collision, const char* collider_value)
{
    if (!ensure_dir("build")) return false;
    if (!ensure_dir("build/testdata")) return false;
    if (!ensure_dir("build/testdata/world_map")) return false;
    if (!write_test_tileset("build/testdata/world_map/tiles.tsx", collider_value)) return false;
    if (two_layers) {
        return write_test_map_two_layers("build/testdata/world_map/map.tmx", "tiles.tsx", layer0_gid, layer1_gid, layer1_collision);
    }
    return write_test_map_one_layer("build/testdata/world_map/map.tmx", "tiles.tsx", layer0_gid, true);
}

void test_world_map_tile_edits_are_queued_until_apply(void)
{
    ensure_clean_world();

    const char* full = "[1111],[1111],[1111],[1111]";
    TEST_ASSERT_TRUE(write_world_testdata(false, 1u, 0u, false, full));
    TEST_ASSERT_TRUE(world_load_from_tmx("build/testdata/world_map/map.tmx", NULL));

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
    TEST_ASSERT_NULL(world_get_map());

    const char* empty = "[0000],[0000],[0000],[0000]";
    TEST_ASSERT_TRUE(write_world_testdata(false, 1u, 0u, false, empty));
    TEST_ASSERT_TRUE(world_load_from_tmx("build/testdata/world_map/map.tmx", NULL));
    uint32_t g1 = world_map_generation();
    TEST_ASSERT_EQUAL_UINT32(g0 + 1u, g1);
    TEST_ASSERT_NOT_NULL(world_get_map());

    TEST_ASSERT_TRUE(write_world_testdata(false, 1u, 0u, false, empty));
    TEST_ASSERT_TRUE(world_load_from_tmx("build/testdata/world_map/map.tmx", NULL));
    uint32_t g2 = world_map_generation();
    TEST_ASSERT_EQUAL_UINT32(g1 + 1u, g2);

    world_shutdown();
    TEST_ASSERT_NULL(world_get_map());
}

void test_world_set_tile_gid_rejects_invalid_inputs(void)
{
    ensure_clean_world();

    const char* full = "[1111],[1111],[1111],[1111]";
    TEST_ASSERT_TRUE(write_world_testdata(true, 1u, 1u, false, full));
    TEST_ASSERT_TRUE(world_load_from_tmx("build/testdata/world_map/map.tmx", NULL));

    world_map_t* map = (world_map_t*)world_get_map();
    TEST_ASSERT_NOT_NULL(map);
    map->layers[1].gids = NULL;

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

    const char* full = "[1111],[1111],[1111],[1111]";
    TEST_ASSERT_TRUE(write_world_testdata(true, 1u, 1u, false, full));
    TEST_ASSERT_TRUE(world_load_from_tmx("build/testdata/world_map/map.tmx", NULL));
    TEST_ASSERT_EQUAL_INT(WORLD_TILE_SOLID, world_tile_at(0, 0));

    // Editing a non-collision layer should not affect the collision grid.
    TEST_ASSERT_TRUE(world_set_tile_gid(1, 0, 0, 0u));
    world_apply_tile_edits();
    TEST_ASSERT_EQUAL_INT(WORLD_TILE_SOLID, world_tile_at(0, 0));
    const world_map_t* m = world_get_map();
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

    test_log_sink_install();
    test_log_sink_reset();
    TEST_ASSERT_FALSE(world_load_from_tmx("build/testdata/does_not_exist.tmx", NULL));
    TEST_ASSERT_TRUE(test_log_sink_contains(LOG_LVL_ERROR, "WORLD", "world: failed to load TMX"));
    test_log_sink_restore();
    TEST_ASSERT_EQUAL_UINT32(g0, world_map_generation());
    TEST_ASSERT_NULL(world_get_map());
}
