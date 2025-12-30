#include "unity.h"

#include "modules/asset/asset.h"
#include "modules/core/logger.h"
#include "asset_backend_stub.h"
#include "test_log_sink.h"

void setUp(void)
{
    test_log_sink_install();
    test_log_sink_reset();
    asset_backend_stub_reset();
    asset_init();
}

void tearDown(void)
{
    asset_shutdown();
    test_log_sink_restore();
}

void test_asset_acquire_caches_textures(void)
{
    tex_handle_t first = asset_acquire_texture("foo");
    TEST_ASSERT_TRUE(asset_texture_valid(first));
    TEST_ASSERT_EQUAL_STRING("foo", asset_texture_path(first));
    int width = -1, height = -1;
    asset_texture_size(first, &width, &height);
    TEST_ASSERT_EQUAL_INT(3, width);
    TEST_ASSERT_EQUAL_INT(3, height);

    tex_handle_t second = asset_acquire_texture("foo");
    TEST_ASSERT_EQUAL_UINT32(2, asset_texture_refcount(second));
    asset_release_texture(first);
    TEST_ASSERT_EQUAL_UINT32(1, asset_texture_refcount(second));

    asset_release_texture(second);
    asset_collect();

    TEST_ASSERT_FALSE(asset_texture_valid(second));
    TEST_ASSERT_EQUAL_INT(1, asset_backend_stub_load_count());
    TEST_ASSERT_EQUAL_INT(1, asset_backend_stub_unload_count());
}

void test_asset_collect_invalidates_handles(void)
{
    tex_handle_t handle = asset_acquire_texture("bar");
    asset_release_texture(handle);
    asset_collect();
    TEST_ASSERT_FALSE(asset_texture_valid(handle));
}

void test_asset_reload_all_uses_backend(void)
{
    tex_handle_t handle = asset_acquire_texture("baz");
    asset_reload_all();
    TEST_ASSERT_EQUAL_INT(1, asset_backend_stub_reload_count());
    asset_release_texture(handle);
    asset_collect();
}

void test_asset_collect_reuses_generations(void)
{
    tex_handle_t first = asset_acquire_texture("cycle");
    asset_release_texture(first);
    asset_collect();

    tex_handle_t second = asset_acquire_texture("cycle");
    TEST_ASSERT_NOT_EQUAL(first.gen, second.gen);
    TEST_ASSERT_EQUAL_INT(2, asset_backend_stub_load_count());
    asset_release_texture(second);
    asset_collect();
}

void test_asset_release_ignores_double_free(void)
{
    tex_handle_t handle = asset_acquire_texture("double");
    asset_release_texture(handle);
    asset_release_texture(handle); // should not underflow refcount
    TEST_ASSERT_EQUAL_UINT32(0, asset_texture_refcount(handle));
    asset_collect();
}

void test_asset_texture_size_invalid_handle_returns_zero(void)
{
    int w = -1, h = -1;
    asset_texture_size((tex_handle_t){0}, &w, &h);
    TEST_ASSERT_EQUAL_INT(0, w);
    TEST_ASSERT_EQUAL_INT(0, h);
}

void test_asset_acquire_null_path_returns_invalid_handle(void)
{
    tex_handle_t h = asset_acquire_texture(NULL);
    TEST_ASSERT_FALSE(asset_texture_valid(h));
    TEST_ASSERT_EQUAL_UINT32(0, h.idx);
    TEST_ASSERT_EQUAL_UINT32(0, h.gen);
}

void test_asset_acquire_reports_backend_failure(void)
{
    asset_backend_stub_fail_next_load();
    tex_handle_t h = asset_acquire_texture("fail");
    TEST_ASSERT_FALSE(asset_texture_valid(h));
    TEST_ASSERT_EQUAL_INT(0, asset_backend_stub_load_count());
    TEST_ASSERT_TRUE(test_log_sink_contains(LOG_LVL_ERROR, NULL, "backend failed to load"));
}
