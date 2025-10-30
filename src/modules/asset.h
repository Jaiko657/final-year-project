#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "raylib.h"

typedef struct { uint32_t idx, gen; } tex_handle_t;  // opaque to ECS

void asset_init(void);
void asset_shutdown(void);
void asset_collect(void);

//DEBUG FEATURES
void asset_reload_all(void);
void asset_log_debug(void);

tex_handle_t asset_acquire_texture(const char* path); // +1 (load or reuse)
void         asset_addref_texture(tex_handle_t h);    // +1
void         asset_release_texture(tex_handle_t h);   // -1 (actual unload deferred to collect)

bool         asset_texture_valid(tex_handle_t h);
void         asset_texture_size(tex_handle_t h, int* out_w, int* out_h);
const char*  asset_texture_path(tex_handle_t h);
uint32_t     asset_texture_refcount(tex_handle_t h);
Texture2D    asset_backend_resolve_texture_value(tex_handle_t h);
