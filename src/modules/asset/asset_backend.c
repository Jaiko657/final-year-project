#include "modules/asset/asset_backend.h"
#include "modules/asset/asset_backend_internal.h"
#include "modules/core/logger.h"

#include "raylib.h"

#include <stdbool.h>
#include <stdlib.h>

struct AssetBackendTexture {
    Texture2D tex;
};

AssetBackendTexture* asset_backend_load_texture(const char* path)
{
    AssetBackendTexture* tex = (AssetBackendTexture*)malloc(sizeof(*tex));
    if (!tex) return NULL;
    tex->tex = LoadTexture(path);
    return tex;
}

void asset_backend_unload_texture(AssetBackendTexture* tex)
{
    if (!tex) return;
    if (tex->tex.id) UnloadTexture(tex->tex);
    free(tex);
}

void asset_backend_texture_size(const AssetBackendTexture* tex, int* out_w, int* out_h)
{
    if (!tex) { if (out_w) *out_w = 0; if (out_h) *out_h = 0; return; }
    if (out_w) *out_w = tex->tex.width;
    if (out_h) *out_h = tex->tex.height;
}

void asset_backend_debug_info(const AssetBackendTexture* tex, AssetBackendDebugInfo* out)
{
    if (!out) return;
    if (!tex) {
        out->id = 0;
        out->width = 0;
        out->height = 0;
        return;
    }
    out->id = tex->tex.id;
    out->width = tex->tex.width;
    out->height = tex->tex.height;
}

void asset_backend_reload_all_begin(void)
{
    LOGC(LOGCAT_ASSET, LOG_LVL_INFO, "Reloading all textures...");
}

void asset_backend_reload_texture(AssetBackendTexture* tex, const char* path)
{
    if (!tex || !path) return;

    Image img = LoadImage(path);
    if (!img.data) {
        LOGC(LOGCAT_ASSET, LOG_LVL_ERROR, "asset: reload failed to load '%s'", path);
        return;
    }

    bool sameSize = (img.width == tex->tex.width) && (img.height == tex->tex.height);

    if (sameSize) {
        if (img.format != tex->tex.format) {
            ImageFormat(&img, tex->tex.format);
        }

        if (img.format == tex->tex.format) {
            UpdateTexture(tex->tex, img.data);
            GenTextureMipmaps(&tex->tex);
        } else {
            Texture2D new_tex = LoadTextureFromImage(img);
            if (new_tex.id) {
                UnloadTexture(tex->tex);
                tex->tex = new_tex;
                GenTextureMipmaps(&tex->tex);
            } else {
                LOGC(LOGCAT_ASSET, LOG_LVL_ERROR, "asset: recreate failed for '%s'", path);
            }
        }
        } else {
            Texture2D new_tex = LoadTextureFromImage(img);
            if (new_tex.id) {
                UnloadTexture(tex->tex);
                tex->tex = new_tex;
                GenTextureMipmaps(&tex->tex);
            } else {
                LOGC(LOGCAT_ASSET, LOG_LVL_ERROR, "asset: recreate failed for '%s' (size changed)", path);
            }
    }

    UnloadImage(img);
}

void asset_backend_reload_all_end(void)
{
    LOGC(LOGCAT_ASSET, LOG_LVL_INFO, "Reload complete.");
}

Texture2D asset_backend_resolve_texture_value(tex_handle_t h)
{
    AssetBackendTexture* tex = asset_backend_lookup_texture(h);
    return tex ? tex->tex : (Texture2D){0};
}
