#include "unity.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "tiled.h"

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

static void ensure_dir(const char *path)
{
#if !defined(_WIN32)
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        TEST_FAIL_MESSAGE("mkdir failed");
    }
#else
    (void)path;
#endif
}

static void write_text_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(f);
    size_t n = strlen(text);
    size_t wrote = fwrite(text, 1, n, f);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)n, (uint32_t)wrote);
    fclose(f);
}

static void setup_tiled_fixture_paths(char *out_dir, size_t out_dir_sz,
                                      char *out_tmx, size_t out_tmx_sz,
                                      char *out_tsx, size_t out_tsx_sz,
                                      char *out_png, size_t out_png_sz)
{
    snprintf(out_dir, out_dir_sz, "build/testdata");
    ensure_dir("build");
    ensure_dir(out_dir);
    snprintf(out_dir, out_dir_sz, "build/testdata/tiled");
    ensure_dir(out_dir);

    snprintf(out_tmx, out_tmx_sz, "%s/map.tmx", out_dir);
    snprintf(out_tsx, out_tsx_sz, "%s/tiles.tsx", out_dir);
    snprintf(out_png, out_png_sz, "%s/tiles.png", out_dir);
}

void test_tiled_load_map_external_tsx_properties_and_animation(void)
{
    char dir[128], tmx[160], tsx[160], png[160];
    setup_tiled_fixture_paths(dir, sizeof(dir), tmx, sizeof(tmx), tsx, sizeof(tsx), png, sizeof(png));

    write_text_file(png, "not a real png, just needs to exist");

    const char *tsx_text =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<tileset name=\"test\" tilewidth=\"32\" tileheight=\"32\" tilecount=\"2\" columns=\"2\">\n"
        "  <image source=\"tiles.png\" width=\"64\" height=\"32\"/>\n"
        "  <tile id=\"0\">\n"
        "    <properties>\n"
        "      <property name=\"collider\" value=\"[1000],[0000],[0000],[0000]\"/>\n"
        "      <property name=\"renderstyle\" value=\"painters\"/>\n"
        "      <property name=\"painteroffset\" value=\"7\"/>\n"
        "      <property name=\"animationtype\" value=\"door\"/>\n"
        "    </properties>\n"
        "    <animation>\n"
        "      <frame tileid=\"1\" duration=\"100\"/>\n"
        "    </animation>\n"
        "  </tile>\n"
        "</tileset>\n";
    write_text_file(tsx, tsx_text);

    const char *tmx_text =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<map width=\"2\" height=\"1\" tilewidth=\"32\" tileheight=\"32\">\n"
        "  <tileset firstgid=\"1\" source=\"tiles.tsx\"/>\n"
        "  <layer name=\"walls\" width=\"2\" height=\"1\">\n"
        "    <properties><property name=\"collision\" value=\"true\"/></properties>\n"
        "    <data>1,0</data>\n"
        "  </layer>\n"
        "</map>\n";
    write_text_file(tmx, tmx_text);

    tiled_map_t map = {0};
    TEST_ASSERT_TRUE(tiled_load_map(tmx, &map));
    TEST_ASSERT_EQUAL_INT(2, map.width);
    TEST_ASSERT_EQUAL_INT(1, map.height);
    TEST_ASSERT_EQUAL_INT(32, map.tilewidth);
    TEST_ASSERT_EQUAL_INT(32, map.tileheight);

    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)map.tileset_count);
    TEST_ASSERT_NOT_NULL(map.tilesets);

    tiled_tileset_t *ts = &map.tilesets[0];
    TEST_ASSERT_EQUAL_INT(1, ts->first_gid);
    TEST_ASSERT_EQUAL_INT(2, ts->tilecount);
    TEST_ASSERT_NOT_NULL(ts->image_path);
    TEST_ASSERT_TRUE(strstr(ts->image_path, "tiles.png") != NULL);

    TEST_ASSERT_NOT_NULL(ts->colliders);
    TEST_ASSERT_EQUAL_HEX16(0x0001, ts->colliders[0]); // row0 col0 bit set

    TEST_ASSERT_NOT_NULL(ts->no_merge_collider);
    TEST_ASSERT_TRUE(ts->no_merge_collider[0]);
    TEST_ASSERT_TRUE(ts->no_merge_collider[1]); // propagated via animation frames

    TEST_ASSERT_NOT_NULL(ts->render_painters);
    TEST_ASSERT_TRUE(ts->render_painters[0]);
    TEST_ASSERT_TRUE(ts->render_painters[1]); // propagated via animation frames

    TEST_ASSERT_NOT_NULL(ts->painter_offset);
    TEST_ASSERT_EQUAL_INT(7, ts->painter_offset[0]);
    TEST_ASSERT_EQUAL_INT(7, ts->painter_offset[1]);

    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)map.layer_count);
    TEST_ASSERT_NOT_NULL(map.layers);
    TEST_ASSERT_TRUE(map.layers[0].collision);
    TEST_ASSERT_NOT_NULL(map.layers[0].gids);
    TEST_ASSERT_EQUAL_UINT32(1, map.layers[0].gids[0]);
    TEST_ASSERT_EQUAL_UINT32(0, map.layers[0].gids[1]);

    tiled_free_map(&map);
}

void test_tiled_load_map_accepts_bom_and_xml_pi(void)
{
    char dir[128], tmx[160], tsx[160], png[160];
    setup_tiled_fixture_paths(dir, sizeof(dir), tmx, sizeof(tmx), tsx, sizeof(tsx), png, sizeof(png));

    write_text_file(png, "x");

    const char *tsx_text =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<tileset name=\"test\" tilewidth=\"32\" tileheight=\"32\" tilecount=\"1\" columns=\"1\">\n"
        "  <image source=\"tiles.png\" width=\"32\" height=\"32\"/>\n"
        "</tileset>\n";
    write_text_file(tsx, tsx_text);

    const char *tmx_text =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<map width=\"1\" height=\"1\" tilewidth=\"32\" tileheight=\"32\">\n"
        "  <tileset firstgid=\"1\" source=\"tiles.tsx\"/>\n"
        "  <layer name=\"L\" width=\"1\" height=\"1\"><data>0</data></layer>\n"
        "</map>\n";
    const uint8_t bom[] = {0xEF, 0xBB, 0xBF};
    FILE *f = fopen(tmx, "wb");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof(bom), (uint32_t)fwrite(bom, 1, sizeof(bom), f));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)strlen(tmx_text), (uint32_t)fwrite(tmx_text, 1, strlen(tmx_text), f));
    fclose(f);

    tiled_map_t map = {0};
    TEST_ASSERT_TRUE(tiled_load_map(tmx, &map));
    TEST_ASSERT_EQUAL_INT(1, map.width);
    tiled_free_map(&map);
}

void test_tiled_load_map_fails_on_layer_csv_mismatch(void)
{
    char dir[128], tmx[160], tsx[160], png[160];
    setup_tiled_fixture_paths(dir, sizeof(dir), tmx, sizeof(tmx), tsx, sizeof(tsx), png, sizeof(png));

    write_text_file(png, "x");
    write_text_file(tsx,
        "<tileset name=\"t\" tilewidth=\"32\" tileheight=\"32\" tilecount=\"1\" columns=\"1\">"
        "<image source=\"tiles.png\" width=\"32\" height=\"32\"/></tileset>"
    );

    // width*height == 2 but only one CSV entry provided.
    write_text_file(tmx,
        "<map width=\"2\" height=\"1\" tilewidth=\"32\" tileheight=\"32\">"
        "<tileset firstgid=\"1\" source=\"tiles.tsx\"/>"
        "<layer name=\"L\" width=\"2\" height=\"1\"><data>0</data></layer>"
        "</map>"
    );

    tiled_map_t map = {0};
    TEST_ASSERT_FALSE(tiled_load_map(tmx, &map));
}

void test_tiled_load_map_inline_tileset_and_chunked_layer(void)
{
    char dir[128], tmx[160], tsx[160], png[160];
    setup_tiled_fixture_paths(dir, sizeof(dir), tmx, sizeof(tmx), tsx, sizeof(tsx), png, sizeof(png));

    (void)tsx;
    write_text_file(png, "x");

    // Inline tileset (no source="...") and chunked data.
    write_text_file(tmx,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<map width=\"2\" height=\"1\" tilewidth=\"32\" tileheight=\"32\">\n"
        "  <tileset firstgid=\"1\" name=\"inline\" tilewidth=\"32\" tileheight=\"32\" tilecount=\"3\" columns=\"3\">\n"
        "    <image source=\"tiles.png\" width=\"64\" height=\"32\"/>\n"
        "    <tile id=\"0\">\n"
        "      <properties>\n"
        "        <property name=\"collider\" value=\"[1000],[0000],[0000],[0000]\"/>\n"
        "        <property name=\"renderstyle\" value=\"painters\"/>\n"
        "        <property name=\"painteroffset\" value=\"7\"/>\n"
        "        <property name=\"animationtype\" value=\"door\"/>\n"
        "        <property name=\"collider\"/>\n"
        "        <property name=\"renderstyle\"/>\n"
        "        <property name=\"painteroffset\"/>\n"
        "      </properties>\n"
        "      <animation>\n"
        "        <frame tileid=\"1\" duration=\"100\"/>\n"
        "        <noop/>\n"
        "        <frame tileid=\"2\" duration=\"-5\"/>\n"
        "      </animation>\n"
        "    </tile>\n"
        "  </tileset>\n"
        "  <layer name=\"L\" width=\"2\" height=\"1\">\n"
        "    <data>\n"
        "      <chunk x=\"0\" y=\"0\" width=\"2\" height=\"1\">1,0</chunk>\n"
        "    </data>\n"
        "  </layer>\n"
        "</map>\n"
    );

    tiled_map_t map = {0};
    TEST_ASSERT_TRUE(tiled_load_map(tmx, &map));
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)map.tileset_count);
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)map.layer_count);

    TEST_ASSERT_NOT_NULL(map.tilesets[0].image_path);
    TEST_ASSERT_TRUE(strstr(map.tilesets[0].image_path, "tiles.png") != NULL);

    tiled_tileset_t *ts0 = &map.tilesets[0];
    TEST_ASSERT_EQUAL_INT(3, ts0->tilecount);
    TEST_ASSERT_NOT_NULL(ts0->colliders);
    TEST_ASSERT_EQUAL_HEX16(0x0001, ts0->colliders[0]);
    TEST_ASSERT_NOT_NULL(ts0->render_painters);
    TEST_ASSERT_TRUE(ts0->render_painters[0]);
    TEST_ASSERT_NOT_NULL(ts0->painter_offset);
    TEST_ASSERT_EQUAL_INT(7, ts0->painter_offset[0]);
    TEST_ASSERT_NOT_NULL(ts0->no_merge_collider);
    TEST_ASSERT_TRUE(ts0->no_merge_collider[0]);

    // Animation parsed and duration clamped for negative values.
    TEST_ASSERT_NOT_NULL(ts0->anims);
    TEST_ASSERT_NOT_NULL(ts0->anims[0].frames);
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)ts0->anims[0].frame_count);
    TEST_ASSERT_EQUAL_INT(100, ts0->anims[0].total_duration_ms);
    TEST_ASSERT_EQUAL_INT(1, ts0->anims[0].frames[0].tile_id);
    TEST_ASSERT_EQUAL_INT(100, ts0->anims[0].frames[0].duration_ms);
    TEST_ASSERT_EQUAL_INT(2, ts0->anims[0].frames[1].tile_id);
    TEST_ASSERT_EQUAL_INT(0, ts0->anims[0].frames[1].duration_ms);

    // Door/no-merge and painter flags propagate to frame tiles.
    TEST_ASSERT_TRUE(ts0->no_merge_collider[1]);
    TEST_ASSERT_TRUE(ts0->no_merge_collider[2]);
    TEST_ASSERT_TRUE(ts0->render_painters[1]);
    TEST_ASSERT_TRUE(ts0->render_painters[2]);
    TEST_ASSERT_EQUAL_INT(7, ts0->painter_offset[1]);
    TEST_ASSERT_EQUAL_INT(7, ts0->painter_offset[2]);

    TEST_ASSERT_NOT_NULL(map.layers[0].gids);
    TEST_ASSERT_EQUAL_UINT32(1, map.layers[0].gids[0]);
    TEST_ASSERT_EQUAL_UINT32(0, map.layers[0].gids[1]);

    tiled_free_map(&map);
}

void test_tiled_object_properties_and_helpers(void)
{
    char dir[128], tmx[160], tsx[160], png[160];
    setup_tiled_fixture_paths(dir, sizeof(dir), tmx, sizeof(tmx), tsx, sizeof(tsx), png, sizeof(png));

    write_text_file(png, "x");
    write_text_file(tsx,
        "<tileset name=\"t\" tilewidth=\"32\" tileheight=\"32\" tilecount=\"1\" columns=\"1\">"
        "<image source=\"tiles.png\" width=\"32\" height=\"32\"/></tileset>"
    );

    // objectgroup with a gid and custom property (case-insensitive lookup)
    write_text_file(tmx,
        "<map width=\"1\" height=\"1\" tilewidth=\"32\" tileheight=\"32\">"
        "<tileset firstgid=\"1\" source=\"tiles.tsx\"/>"
        "<layer name=\"L\" width=\"1\" height=\"1\"><data>0</data></layer>"
        "<objectgroup name=\"entities\">"
        "  <object id=\"7\" name=\"coin\" gid=\"1\" x=\"16\" y=\"32\" width=\"0\" height=\"0\">"
        "    <properties>"
        "      <property name=\"Foo\" type=\"string\" value=\"Bar\"/>"
        "      <property name=\"proximity_radius\" type=\"int\" value=\"123\"/>"
        "    </properties>"
        "  </object>"
        "</objectgroup>"
        "</map>"
    );

    tiled_map_t map = {0};
    TEST_ASSERT_TRUE(tiled_load_map(tmx, &map));
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)map.object_count);

    tiled_object_t *o = &map.objects[0];
    TEST_ASSERT_EQUAL_INT(7, o->id);
    TEST_ASSERT_EQUAL_UINT32(1, o->gid);
    TEST_ASSERT_NOT_NULL(o->layer_name);
    TEST_ASSERT_EQUAL_STRING("entities", o->layer_name);
    TEST_ASSERT_NOT_NULL(o->name);
    TEST_ASSERT_EQUAL_STRING("coin", o->name);

    // helpers in tiled.c
    TEST_ASSERT_EQUAL_STRING("Bar", tiled_object_get_property_value(o, "foo"));
    TEST_ASSERT_EQUAL_STRING("123", tiled_object_get_property_value(o, "proximity_radius"));
    TEST_ASSERT_NULL(tiled_object_get_property_value(o, "missing"));

    tiled_free_map(&map);
}

void test_tiled_normalizes_relative_tsx_paths_and_parses_empty_bool_property(void)
{
    char dir[128], tmx[160], tsx[160], png[160];
    setup_tiled_fixture_paths(dir, sizeof(dir), tmx, sizeof(tmx), tsx, sizeof(tsx), png, sizeof(png));

    // Put TSX in the base dir, but reference it as "sub/../tiles.tsx" from TMX.
    ensure_dir("build/testdata/tiled/sub");

    write_text_file(png, "x");
    write_text_file(tsx,
        "<tileset name=\"t\" tilewidth=\"32\" tileheight=\"32\" tilecount=\"1\" columns=\"1\">"
        "<image source=\"tiles.png\" width=\"32\" height=\"32\"/></tileset>"
    );

    write_text_file(tmx,
        "<map width=\"1\" height=\"1\" tilewidth=\"32\" tileheight=\"32\">"
        "<tileset firstgid=\"1\" source=\"sub/../tiles.tsx\"/>"
        "<layer name=\"L\" width=\"1\" height=\"1\">"
        "  <properties><property name=\"collision\" value=\"\"/></properties>"
        "  <data>0</data>"
        "</layer>"
        "</map>"
    );

    tiled_map_t map = {0};
    TEST_ASSERT_TRUE(tiled_load_map(tmx, &map));
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)map.layer_count);
    TEST_ASSERT_TRUE(map.layers[0].collision); // empty string counts as true
    tiled_free_map(&map);
}

void test_tiled_tileset_parse_failure_falls_back_to_scanning_image_source(void)
{
    char dir[128], tmx[160], tsx[160], png[160];
    setup_tiled_fixture_paths(dir, sizeof(dir), tmx, sizeof(tmx), tsx, sizeof(tsx), png, sizeof(png));

    write_text_file(png, "x");

    // Missing required tileset attributes (e.g. tilecount/columns), so parse_tileset() fails.
    // tiled_load_map() should then scan the TSX for <image source="..."> and still accept it
    // as long as the image path resolves and exists.
    write_text_file(tsx,
        "<tileset name=\"broken\" tilewidth=\"32\" tileheight=\"32\">"
        "<image source=\"tiles.png\" width=\"32\" height=\"32\"/>"
        "</tileset>"
    );

    write_text_file(tmx,
        "<map width=\"1\" height=\"1\" tilewidth=\"32\" tileheight=\"32\">"
        "<tileset firstgid=\"1\" source=\"tiles.tsx\"/>"
        "<layer name=\"L\" width=\"1\" height=\"1\"><data>0</data></layer>"
        "</map>"
    );

    tiled_map_t map = {0};
    TEST_ASSERT_TRUE(tiled_load_map(tmx, &map));
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)map.tileset_count);
    TEST_ASSERT_NOT_NULL(map.tilesets[0].image_path);
    TEST_ASSERT_TRUE(strstr(map.tilesets[0].image_path, "tiles.png") != NULL);
    tiled_free_map(&map);
}

void test_tiled_load_map_fails_when_inline_tileset_has_no_image(void)
{
    char dir[128], tmx[160], tsx[160], png[160];
    setup_tiled_fixture_paths(dir, sizeof(dir), tmx, sizeof(tmx), tsx, sizeof(tsx), png, sizeof(png));
    (void)tsx; (void)png;

    write_text_file(tmx,
        "<map width=\"1\" height=\"1\" tilewidth=\"32\" tileheight=\"32\">"
        "<tileset firstgid=\"1\" name=\"inline\" tilewidth=\"32\" tileheight=\"32\" tilecount=\"1\" columns=\"1\">"
        "</tileset>"
        "<layer name=\"L\" width=\"1\" height=\"1\"><data>0</data></layer>"
        "</map>"
    );

    tiled_map_t map = {0};
    TEST_ASSERT_FALSE(tiled_load_map(tmx, &map));
}

void test_tiled_inline_tileset_scan_attr_in_file_fallback_for_missing_image_source(void)
{
    char dir[128], tmx[160], tsx[160], png[160];
    setup_tiled_fixture_paths(dir, sizeof(dir), tmx, sizeof(tmx), tsx, sizeof(tsx), png, sizeof(png));
    (void)tsx;

    write_text_file(png, "x");

    // Omit the source attribute from the <image> node but include a `source="..."` string
    // in the node content so scan_attr_in_file() still finds it. This exercises that
    // fallback path without relying on XML comments support.
    write_text_file(tmx,
        "<map width=\"1\" height=\"1\" tilewidth=\"32\" tileheight=\"32\">"
        "<tileset firstgid=\"1\" name=\"inline\" tilewidth=\"32\" tileheight=\"32\" tilecount=\"1\" columns=\"1\">"
        "<image width=\"32\" height=\"32\">source=\"tiles.png\"</image>"
        "</tileset>"
        "<layer name=\"L\" width=\"1\" height=\"1\"><data>0</data></layer>"
        "</map>"
    );

    tiled_map_t map = {0};
    TEST_ASSERT_TRUE(tiled_load_map(tmx, &map));
    TEST_ASSERT_NOT_NULL(map.tilesets[0].image_path);
    TEST_ASSERT_TRUE(strstr(map.tilesets[0].image_path, "tiles.png") != NULL);
    tiled_free_map(&map);
}

void test_tiled_tsx_scan_attr_in_file_fallback_for_missing_image_source(void)
{
    char dir[128], tmx[160], tsx[160], png[160];
    setup_tiled_fixture_paths(dir, sizeof(dir), tmx, sizeof(tmx), tsx, sizeof(tsx), png, sizeof(png));

    write_text_file(png, "x");

    write_text_file(tsx,
        "<tileset name=\"test\" tilewidth=\"32\" tileheight=\"32\" tilecount=\"1\" columns=\"1\">"
        "<image width=\"32\" height=\"32\"><!-- source=\"tiles.png\" --></image>"
        "</tileset>"
    );

    write_text_file(tmx,
        "<map width=\"1\" height=\"1\" tilewidth=\"32\" tileheight=\"32\">"
        "<tileset firstgid=\"1\" source=\"tiles.tsx\"/>"
        "<layer name=\"L\" width=\"1\" height=\"1\"><data>0</data></layer>"
        "</map>"
    );

    tiled_map_t map = {0};
    TEST_ASSERT_TRUE(tiled_load_map(tmx, &map));
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)map.tileset_count);
    TEST_ASSERT_NOT_NULL(map.tilesets[0].image_path);
    TEST_ASSERT_TRUE(strstr(map.tilesets[0].image_path, "tiles.png") != NULL);
    tiled_free_map(&map);
}
