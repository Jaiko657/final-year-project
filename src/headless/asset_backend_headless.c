#include "modules/asset/asset_backend.h"
#include "modules/core/logger.h"
#include "modules/common/resource_handles.h"

#include "raylib.h"

#include <stdlib.h>

struct AssetBackendTexture {
    uint32_t id;
    int width;
    int height;
};

AssetBackendTexture* asset_backend_load_texture(const char* path)
{
    (void)path;
    AssetBackendTexture* tex = (AssetBackendTexture*)malloc(sizeof(*tex));
    if (!tex) return NULL;
    tex->id = 0;
    tex->width = 0;
    tex->height = 0;
    return tex;
}

void asset_backend_unload_texture(AssetBackendTexture* tex)
{
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
    LOGC(LOGCAT_ASSET, LOG_LVL_INFO, "headless asset: reload_all ignored");
}

void asset_backend_reload_texture(AssetBackendTexture* tex, const char* path)
{
    (void)tex;
    (void)path;
}

void asset_backend_reload_all_end(void)
{
}

Texture2D asset_backend_resolve_texture_value(tex_handle_t h)
{
    (void)h;
    return (Texture2D){0};
}
