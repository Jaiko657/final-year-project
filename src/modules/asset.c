#include "../includes/asset.h"
#include "../includes/logger.h"
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "raylib.h"

typedef struct Slot {
    Texture2D tex;
    uint32_t  gen;
    bool      used;
    uint32_t  refc;
    char*     path;
} Slot;

static Slot s_tex[MAX_TEX];

static Slot* slot_from_handle(tex_handle_t h) {
    if (h.idx >= MAX_TEX) return NULL;
    Slot* s = &s_tex[h.idx];
    if (!s->used || s->gen != h.gen) return NULL;
    return s;
}

static tex_handle_t make_handle(uint32_t idx, uint32_t gen){
    tex_handle_t h; h.idx = idx; h.gen = gen; return h;
}

void asset_init(void){
    memset(s_tex, 0, sizeof(s_tex));
}

void asset_shutdown(void){
    // Force-release everything
    for (int i=0;i<MAX_TEX;i++){
        Slot* s = &s_tex[i];
        if (s->used){
            if (s->tex.id) UnloadTexture(s->tex);
            free(s->path);
        }
    }
    memset(s_tex, 0, sizeof(s_tex));
}

void asset_collect(void){
    // Unload any zero-ref textures but keep slot for reuse (bump gen when freeing)
    for (int i=0;i<MAX_TEX;i++){
        Slot* s = &s_tex[i];
        if (s->used && s->refc == 0){
            if (s->tex.id) UnloadTexture(s->tex);
            s->tex = (Texture2D){0};
            free(s->path); s->path = NULL;
            s->used = false;
            s->gen++;
            // leave slot cleared for reuse
        }
    }
}

static int find_by_path(const char* path){
    for (int i=0;i<MAX_TEX;i++){
        Slot* s = &s_tex[i];
        if (s->used && s->path && strcmp(s->path, path)==0) return i;
    }
    return -1;
}
//trying to not use gnu util
static char* xstrdup(const char* s){
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

tex_handle_t asset_acquire_texture(const char* path){
    // Try cache first
    int idx = find_by_path(path);
    if (idx >= 0){
        Slot* s = &s_tex[idx];
        s->refc++;
        return make_handle((uint32_t)idx, s->gen);
    }

    // Find free slot
    for (int i=0;i<MAX_TEX;i++){
        if (!s_tex[i].used){
            Slot* s = &s_tex[i];
            s->tex = LoadTexture(path);
            s->used = true;
            s->refc = 1;
            free(s->path);
            s->path = xstrdup(path);
            // gen stays as-is (starts zero), only increases when we invalidate
            return make_handle((uint32_t)i, s->gen);
        }
    }

    LOG(LOG_LVL_ERROR, "asset: out of texture slots (MAX_TEX=%i)", MAX_TEX);
    return (tex_handle_t){0};
}

void asset_addref_texture(tex_handle_t h){
    Slot* s = slot_from_handle(h);
    if (s) s->refc++;
}

void asset_release_texture(tex_handle_t h){
    Slot* s = slot_from_handle(h);
    if (!s) return;
    if (s->refc == 0) return; // already zeroed; ignore double-free
    s->refc--;
}

bool asset_texture_valid(tex_handle_t h){
    return slot_from_handle(h) != NULL;
}

void asset_texture_size(tex_handle_t h, int* out_w, int* out_h){
    Slot* s = slot_from_handle(h);
    if (!s){ if(out_w)*out_w=0; if(out_h)*out_h=0; return; }
    if (out_w) *out_w = s->tex.width;
    if (out_h) *out_h = s->tex.height;
}

const char* asset_texture_path(tex_handle_t h){
    Slot* s = slot_from_handle(h);
    return s ? s->path : NULL;
}

uint32_t asset_texture_refcount(tex_handle_t h){
    Slot* s = slot_from_handle(h);
    return s ? s->refc : 0;
}

// This is internal to here and renderer, so is exposed in includes/asset_renderer_internal.h
Texture2D asset_backend_resolve_texture_value(tex_handle_t h){
    Slot* s = slot_from_handle(h);
    return s ? s->tex : (Texture2D){0};
}

void asset_log_debug(void) {
    LOGC(LOGCAT_ASSET, LOG_LVL_DEBUG, "---- Asset Texture Debug Dump ----");
    for (int i = 0; i < MAX_TEX; i++) {
        Slot* s = &s_tex[i];
        if (s->used) {
            LOGC(LOGCAT_ASSET, LOG_LVL_DEBUG,
                 "[%04d] gen=%u refc=%u path=\"%s\" tex.id=%u (%dx%d)",
                 i,
                 s->gen,
                 s->refc,
                 s->path ? s->path : "(null)",
                 s->tex.id,
                 s->tex.width,
                 s->tex.height);
        }
    }
    LOGC(LOGCAT_ASSET, LOG_LVL_DEBUG, "----------------------------------");
}

void asset_reload_all(void){
    LOGC(LOGCAT_ASSET, LOG_LVL_INFO, "Reloading all textures...");
    for (int i = 0; i < MAX_TEX; i++) {
        Slot* s = &s_tex[i];
        if (!s->used || !s->path) continue;

        Image img = LoadImage(s->path);
        if (!img.data) {
            LOG(LOG_LVL_ERROR, "asset: reload failed to load '%s'", s->path);
            continue;
        }

        bool sameSize = (img.width == s->tex.width) && (img.height == s->tex.height);

        if (sameSize) {
            if (img.format != s->tex.format) {
                ImageFormat(&img, s->tex.format); // converts in-place; if format not supported it’s a no-op
            }

            if (img.format == s->tex.format) {
                UpdateTexture(s->tex, img.data);
                GenTextureMipmaps(&s->tex);
            } else {
                Texture2D new_tex = LoadTextureFromImage(img);
                if (new_tex.id) {
                    UnloadTexture(s->tex);
                    s->tex = new_tex;
                    GenTextureMipmaps(&s->tex);
                } else {
                    LOG(LOG_LVL_ERROR, "asset: recreate failed for '%s'", s->path);
                }
            }
        } else {
            Texture2D new_tex = LoadTextureFromImage(img);
            if (new_tex.id) {
                UnloadTexture(s->tex);
                s->tex = new_tex;
                GenTextureMipmaps(&s->tex);
            } else {
                LOG(LOG_LVL_ERROR, "asset: recreate failed for '%s' (size changed)", s->path);
            }
        }

        UnloadImage(img);
    }
    LOGC(LOGCAT_ASSET, LOG_LVL_INFO, "Reload complete.");
}
