#include "modules/asset/asset_backend.h"
#include "asset_backend_stub.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct AssetBackendTexture {
    uint32_t id;
    int width;
    int height;
};

static int g_load_count;
static int g_unload_count;
static int g_reload_count;
static uint32_t g_next_id;
static bool g_next_load_fail;

void asset_backend_stub_reset(void)
{
    g_load_count = 0;
    g_unload_count = 0;
    g_reload_count = 0;
    g_next_id = 1;
    g_next_load_fail = false;
}

void asset_backend_stub_fail_next_load(void)
{
    g_next_load_fail = true;
}

int asset_backend_stub_load_count(void)
{
    return g_load_count;
}

int asset_backend_stub_unload_count(void)
{
    return g_unload_count;
}

int asset_backend_stub_reload_count(void)
{
    return g_reload_count;
}

AssetBackendTexture* asset_backend_load_texture(const char* path)
{
    if (g_next_load_fail) {
        g_next_load_fail = false;
        return NULL;
    }
    AssetBackendTexture* tex = (AssetBackendTexture*)malloc(sizeof(*tex));
    if (!tex) return NULL;
    tex->id = g_next_id++;
    tex->width = path ? (int)strlen(path) : 0;
    tex->height = tex->width;
    g_load_count++;
    (void)path;
    return tex;
}

void asset_backend_unload_texture(AssetBackendTexture* tex)
{
    if (!tex) return;
    g_unload_count++;
    free(tex);
}

void asset_backend_texture_size(const AssetBackendTexture* tex, int* out_w, int* out_h)
{
    if (!tex) { if (out_w) *out_w = 0; if (out_h) *out_h = 0; return; }
    if (out_w) *out_w = tex->width;
    if (out_h) *out_h = tex->height;
}

void asset_backend_debug_info(const AssetBackendTexture* tex, AssetBackendDebugInfo* out)
{
    if (!out) return;
    out->id = tex ? tex->id : 0;
    out->width = tex ? tex->width : 0;
    out->height = tex ? tex->height : 0;
}

void asset_backend_reload_all_begin(void)
{
}

void asset_backend_reload_texture(AssetBackendTexture* tex, const char* path)
{
    (void)path;
    if (!tex) return;
    g_reload_count++;
    tex->width = tex->height = tex->width + 1;
}

void asset_backend_reload_all_end(void)
{
}
