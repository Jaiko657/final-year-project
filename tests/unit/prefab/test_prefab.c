#include "unity.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "modules/prefab/prefab.h"

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

static void setup_prefab_fixture_path(char *out_dir, size_t out_dir_sz, char *out_path, size_t out_path_sz)
{
    snprintf(out_dir, out_dir_sz, "build/testdata");
    ensure_dir("build");
    ensure_dir(out_dir);
    snprintf(out_dir, out_dir_sz, "build/testdata/prefab");
    ensure_dir(out_dir);

    snprintf(out_path, out_path_sz, "%s/test.ent", out_dir);
}

static const prefab_component_t *find_comp(const prefab_t *p, ComponentEnum id)
{
    if (!p || !p->components) return NULL;
    for (size_t i = 0; i < p->component_count; ++i) {
        if (p->components[i].id == id) return &p->components[i];
    }
    return NULL;
}

static const char *find_prop(const prefab_component_t *c, const char *name)
{
    if (!c || !name) return NULL;
    for (size_t i = 0; i < c->prop_count; ++i) {
        if (c->props[i].name && strcmp(c->props[i].name, name) == 0) return c->props[i].value;
    }
    return NULL;
}

void test_prefab_load_parses_all_component_types_and_varieties(void)
{
    char dir[128], path[160];
    setup_prefab_fixture_path(dir, sizeof(dir), path, sizeof(path));

    const char *xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<prefab name=\"everything\">\n"
        "  <component type=\"POS\" x=\"1\" y=\"2\"/>\n"
        "  <component type=\"VEL\" x=\"3\" y=\"4\" dir=\"north\"/>\n"
        "  <component type=\"PHYS_BODY\"><property name=\"type\" value=\"dynamic\"/></component>\n"
        "  <component type=\"SPR\" path=\"tex.png\" src=\"0,0,16,16\"/>\n"
        "  <component type=\"ANIM\" frame_w=\"16\" frame_h=\"16\" fps=\"6\">\n"
        "    <anim name=\"walk\">\n"
        "      <frame col=\"0\" row=\"0\"/>\n"
        "      <frame col=\"1\" row=\"0\"/>\n"
        "    </anim>\n"
        "    <anim name=\"idle\">\n"
        "      <frame col=\"2\" row=\"1\"/>\n"
        "    </anim>\n"
        "  </component>\n"
        "  <component type=\"PLAYER\"/>\n"
        "  <component type=\"ITEM\"><property name=\"kind\">coin</property></component>\n"
        "  <component type=\"INV\"><property name=\"empty\"/></component>\n"
        "  <component type=\"VENDOR\" sells=\"hat\" price=\"5\"/>\n"
        "  <component type=\"FOLLOW\" target=\"player\" desired_distance=\"10\" max_speed=\"20\" vision_range=\"30\"/>\n"
        "  <component type=\"COL\" hx=\"8\" hy=\"9\"/>\n"
        "  <component type=\"LIFTABLE\"><property name=\"bounce_damping\" value=\"0.5\"/></component>\n"
        "  <component type=\"TRIGGER\" pad=\"0\" target_mask=\"ITEM|COL\"/>\n"
        "  <component type=\"BILLBOARD\" state=\"active\"><property name=\"text\" value=\"hi\"/></component>\n"
        "  <component type=\"DOOR\" proximity_radius=\"12\" door_tiles=\"1,2;3,4\"/>\n"
        "</prefab>\n";
    write_text_file(path, xml);

    prefab_t p = {0};
    TEST_ASSERT_TRUE(prefab_load(path, &p));
    TEST_ASSERT_NOT_NULL(p.name);
    TEST_ASSERT_EQUAL_STRING("everything", p.name);
    TEST_ASSERT_EQUAL_UINT32(15, (uint32_t)p.component_count);

    const prefab_component_t *pos = find_comp(&p, ENUM_POS);
    TEST_ASSERT_NOT_NULL(pos);
    TEST_ASSERT_EQUAL_STRING("POS", pos->type_name);
    TEST_ASSERT_EQUAL_STRING("1", find_prop(pos, "x"));
    TEST_ASSERT_EQUAL_STRING("2", find_prop(pos, "y"));

    const prefab_component_t *phys = find_comp(&p, ENUM_PHYS_BODY);
    TEST_ASSERT_NOT_NULL(phys);
    TEST_ASSERT_EQUAL_STRING("dynamic", find_prop(phys, "type"));

    const prefab_component_t *spr = find_comp(&p, ENUM_SPR);
    TEST_ASSERT_NOT_NULL(spr);
    TEST_ASSERT_EQUAL_STRING("tex.png", find_prop(spr, "path"));
    TEST_ASSERT_EQUAL_STRING("0,0,16,16", find_prop(spr, "src"));

    const prefab_component_t *player = find_comp(&p, ENUM_PLAYER);
    TEST_ASSERT_NOT_NULL(player);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)player->prop_count);

    const prefab_component_t *item = find_comp(&p, ENUM_ITEM);
    TEST_ASSERT_NOT_NULL(item);
    TEST_ASSERT_EQUAL_STRING("coin", find_prop(item, "kind"));

    const prefab_component_t *inv = find_comp(&p, ENUM_INV);
    TEST_ASSERT_NOT_NULL(inv);
    TEST_ASSERT_EQUAL_STRING("", find_prop(inv, "empty"));

    const prefab_component_t *anim = find_comp(&p, ENUM_ANIM);
    TEST_ASSERT_NOT_NULL(anim);
    TEST_ASSERT_NOT_NULL(anim->anim);
    TEST_ASSERT_EQUAL_INT(16, anim->anim->frame_w);
    TEST_ASSERT_EQUAL_INT(16, anim->anim->frame_h);
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)anim->anim->seq_count);
    TEST_ASSERT_EQUAL_STRING("walk", anim->anim->seqs[0].name);
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)anim->anim->seqs[0].frame_count);
    TEST_ASSERT_EQUAL_UINT8(0, anim->anim->seqs[0].frames[0].col);
    TEST_ASSERT_EQUAL_UINT8(0, anim->anim->seqs[0].frames[0].row);
    TEST_ASSERT_EQUAL_STRING("idle", anim->anim->seqs[1].name);
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)anim->anim->seqs[1].frame_count);

    prefab_free(&p);
    prefab_free(NULL);
}

void test_prefab_load_accepts_bom_and_xml_pi(void)
{
    char dir[128], path[160];
    setup_prefab_fixture_path(dir, sizeof(dir), path, sizeof(path));

    const char *xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<prefab name=\"bom\">\n"
        "  <component type=\"POS\" x=\"0\" y=\"0\"/>\n"
        "</prefab>\n";

    const uint8_t bom[] = {0xEF, 0xBB, 0xBF};
    FILE *f = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof(bom), (uint32_t)fwrite(bom, 1, sizeof(bom), f));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)strlen(xml), (uint32_t)fwrite(xml, 1, strlen(xml), f));
    fclose(f);

    prefab_t p = {0};
    TEST_ASSERT_TRUE(prefab_load(path, &p));
    TEST_ASSERT_EQUAL_STRING("bom", p.name);
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)p.component_count);
    prefab_free(&p);
}

void test_prefab_load_fails_on_missing_file(void)
{
    prefab_t p = {0};
    TEST_ASSERT_FALSE(prefab_load("build/testdata/prefab/does_not_exist.ent", &p));
}

void test_prefab_load_fails_on_invalid_xml(void)
{
    char dir[128], path[160];
    setup_prefab_fixture_path(dir, sizeof(dir), path, sizeof(path));
    write_text_file(path, "<prefab><component></prefab>");

    prefab_t p = {0};
    TEST_ASSERT_FALSE(prefab_load(path, &p));
}

void test_prefab_load_fails_when_root_not_prefab(void)
{
    char dir[128], path[160];
    setup_prefab_fixture_path(dir, sizeof(dir), path, sizeof(path));
    write_text_file(path, "<notprefab name=\"x\"><component type=\"POS\"/></notprefab>");

    prefab_t p = {0};
    TEST_ASSERT_FALSE(prefab_load(path, &p));
}

void test_prefab_load_fails_on_component_missing_type(void)
{
    char dir[128], path[160];
    setup_prefab_fixture_path(dir, sizeof(dir), path, sizeof(path));
    write_text_file(path, "<prefab name=\"x\"><component x=\"1\"/></prefab>");

    prefab_t p = {0};
    TEST_ASSERT_FALSE(prefab_load(path, &p));
}

void test_prefab_load_fails_on_unknown_component_type(void)
{
    char dir[128], path[160];
    setup_prefab_fixture_path(dir, sizeof(dir), path, sizeof(path));
    write_text_file(path, "<prefab name=\"x\"><component type=\"NOPE\"/></prefab>");

    prefab_t p = {0};
    TEST_ASSERT_FALSE(prefab_load(path, &p));
}

void test_prefab_load_fails_when_trimmed_is_empty_after_bom(void)
{
    char dir[128], path[160];
    setup_prefab_fixture_path(dir, sizeof(dir), path, sizeof(path));

    const uint8_t bom[] = {0xEF, 0xBB, 0xBF};
    FILE *f = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof(bom), (uint32_t)fwrite(bom, 1, sizeof(bom), f));
    fclose(f);

    prefab_t p = {0};
    TEST_ASSERT_FALSE(prefab_load(path, &p));
}
