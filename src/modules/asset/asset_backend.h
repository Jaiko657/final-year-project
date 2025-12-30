#pragma once

#include <stdint.h>

typedef struct AssetBackendTexture AssetBackendTexture;

typedef struct {
    uint32_t id;
    int width;
    int height;
} AssetBackendDebugInfo;

AssetBackendTexture* asset_backend_load_texture(const char* path);
void asset_backend_unload_texture(AssetBackendTexture* tex);
void asset_backend_texture_size(const AssetBackendTexture* tex, int* out_w, int* out_h);
void asset_backend_debug_info(const AssetBackendTexture* tex, AssetBackendDebugInfo* out);

void asset_backend_reload_all_begin(void);
void asset_backend_reload_texture(AssetBackendTexture* tex, const char* path);
void asset_backend_reload_all_end(void);
