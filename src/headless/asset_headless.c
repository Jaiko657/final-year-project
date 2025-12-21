#include "../includes/asset.h"
#include "../includes/logger.h"

#include <stdlib.h>
#include <string.h>

typedef struct Slot {
    uint32_t  gen;
    bool      used;
    uint32_t  refc;
    char*     path;
} Slot;

static Slot s_tex[MAX_TEX];

static Slot* slot_from_handle(tex_handle_t h)
{
    if (h.idx >= MAX_TEX) return NULL;
    Slot* s = &s_tex[h.idx];
    if (!s->used || s->gen != h.gen) return NULL;
    return s;
}

static tex_handle_t make_handle(uint32_t idx, uint32_t gen)
{
    tex_handle_t h; h.idx = idx; h.gen = gen; return h;
}

static char* xstrdup(const char* s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

void asset_init(void)
{
    memset(s_tex, 0, sizeof(s_tex));
}

void asset_shutdown(void)
{
    for (int i = 0; i < MAX_TEX; ++i) {
        free(s_tex[i].path);
    }
    memset(s_tex, 0, sizeof(s_tex));
}

void asset_collect(void)
{
    for (int i = 0; i < MAX_TEX; ++i) {
        Slot* s = &s_tex[i];
        if (s->used && s->refc == 0) {
            free(s->path); s->path = NULL;
            s->used = false;
            s->gen++;
        }
    }
}

void asset_reload_all(void)
{
    LOGC(LOGCAT_ASSET, LOG_LVL_INFO, "headless asset: reload_all ignored");
}

void asset_log_debug(void)
{
    LOGC(LOGCAT_ASSET, LOG_LVL_DEBUG, "headless asset: %d slots", MAX_TEX);
    for (int i = 0; i < MAX_TEX; ++i) {
        Slot* s = &s_tex[i];
        if (!s->used) continue;
        LOGC(LOGCAT_ASSET, LOG_LVL_DEBUG, "[%04d] gen=%u refc=%u path=\"%s\"",
             i, s->gen, s->refc, s->path ? s->path : "(null)");
    }
}

static int find_by_path(const char* path)
{
    for (int i = 0; i < MAX_TEX; ++i) {
        Slot* s = &s_tex[i];
        if (s->used && s->path && strcmp(s->path, path) == 0) return i;
    }
    return -1;
}

tex_handle_t asset_acquire_texture(const char* path)
{
    if (!path) return (tex_handle_t){0};

    int idx = find_by_path(path);
    if (idx >= 0) {
        Slot* s = &s_tex[idx];
        s->refc++;
        return make_handle((uint32_t)idx, s->gen);
    }

    for (int i = 0; i < MAX_TEX; ++i) {
        if (!s_tex[i].used) {
            Slot* s = &s_tex[i];
            s->used = true;
            s->refc = 1;
            free(s->path);
            s->path = xstrdup(path);
            return make_handle((uint32_t)i, s->gen);
        }
    }

    LOG(LOG_LVL_ERROR, "headless asset: out of texture slots (MAX_TEX=%i)", MAX_TEX);
    return (tex_handle_t){0};
}

void asset_addref_texture(tex_handle_t h)
{
    Slot* s = slot_from_handle(h);
    if (s) s->refc++;
}

void asset_release_texture(tex_handle_t h)
{
    Slot* s = slot_from_handle(h);
    if (!s) return;
    if (s->refc == 0) return;
    s->refc--;
}

bool asset_texture_valid(tex_handle_t h)
{
    return slot_from_handle(h) != NULL;
}

void asset_texture_size(tex_handle_t h, int* out_w, int* out_h)
{
    (void)h;
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
}

const char* asset_texture_path(tex_handle_t h)
{
    Slot* s = slot_from_handle(h);
    return s ? s->path : NULL;
}

uint32_t asset_texture_refcount(tex_handle_t h)
{
    Slot* s = slot_from_handle(h);
    return s ? s->refc : 0;
}

