#include "../includes/world.h"
#include "../includes/logger.h"

#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORLD_TILE_SIZE 16

typedef struct {
    int w, h;          // tiles
    int tile_size;     // pixels per tile
    world_tile_t* tiles;
    v2f spawn;         // pixel position at tile center
    bool has_spawn;
} world_state_t;

static world_state_t g_world = { .tile_size = WORLD_TILE_SIZE };

int world_tile_size(void) {
    return g_world.tile_size;
}

void world_shutdown(void) {
    free(g_world.tiles);
    g_world = (world_state_t){ .tile_size = WORLD_TILE_SIZE };
}

static v2f tile_center_px(int tx, int ty, int tile_size) {
    return v2f_make((tx + 0.5f) * (float)tile_size, (ty + 0.5f) * (float)tile_size);
}

static int decode_tile_char(char c, world_tile_t* out_tile, bool* out_spawn) {
    if (isspace((unsigned char)c)) return 0; // ignore

    switch (c) {
        case '0':
        case '.':
            *out_tile = WORLD_TILE_WALKABLE;
            *out_spawn = false;
            return 1;
        case '1':
        case '#':
            *out_tile = WORLD_TILE_SOLID;
            *out_spawn = false;
            return 1;
        case 'S':
        case 's':
            *out_tile = WORLD_TILE_WALKABLE;
            *out_spawn = true;
            return 1;
        default:
            return -1; // invalid
    }
}

bool world_load(const char* path) {
    world_shutdown(); // drop previous data if any

    FILE* f = fopen(path, "rb");
    if (!f) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "world: failed to open '%s'", path);
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "world: failed to seek '%s'", path);
        fclose(f);
        return false;
    }
    long len = ftell(f);
    if (len < 0) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "world: failed to size '%s'", path);
        fclose(f);
        return false;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "world: failed to rewind '%s'", path);
        fclose(f);
        return false;
    }

    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "world: out of memory reading '%s'", path);
        fclose(f);
        return false;
    }
    size_t read = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[read] = '\0';

    char* cursor = buf;
    errno = 0;
    long w = strtol(cursor, &cursor, 10);
    long h = strtol(cursor, &cursor, 10);
    if (errno != 0 || w <= 0 || h <= 0 || w > 32768 || h > 32768) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "world: invalid width/height in '%s'", path);
        free(buf);
        return false;
    }

    size_t count = (size_t)w * (size_t)h;
    world_tile_t* tiles = (world_tile_t*)malloc(count * sizeof(world_tile_t));
    if (!tiles) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "world: out of memory for %ldx%ld map", w, h);
        free(buf);
        return false;
    }

    size_t idx = 0;
    bool found_spawn = false;

    for (; *cursor != '\0' && idx < count; ++cursor) {
        world_tile_t tile = WORLD_TILE_VOID;
        bool spawn_here = false;
        int res = decode_tile_char(*cursor, &tile, &spawn_here);
        if (res < 0) {
            LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "world: unexpected character '%c' in '%s'", *cursor, path);
            free(buf);
            free(tiles);
            return false;
        } else if (res == 0) {
            continue; // whitespace
        }

        tiles[idx] = tile;
        if (spawn_here) {
            int tx = (int)(idx % (size_t)w);
            int ty = (int)(idx / (size_t)w);
            g_world.spawn = tile_center_px(tx, ty, WORLD_TILE_SIZE);
            found_spawn = true;
        }
        idx++;
    }

    free(buf);

    if (idx != count) {
        LOGC(LOGCAT_MAIN, LOG_LVL_ERROR, "world: map '%s' ended early (expected %zu tiles, got %zu)", path, count, idx);
        free(tiles);
        return false;
    }

    g_world.w = (int)w;
    g_world.h = (int)h;
    g_world.tiles = tiles;
    g_world.tile_size = WORLD_TILE_SIZE;
    g_world.has_spawn = found_spawn;
    if (!g_world.has_spawn) {
        g_world.spawn = tile_center_px(g_world.w / 2, g_world.h / 2, g_world.tile_size);
    }

    LOGC(LOGCAT_MAIN, LOG_LVL_INFO, "world: loaded %dx%d tiles from '%s'%s", g_world.w, g_world.h, path, found_spawn ? " (spawn set)" : "");
    return true;
}

void world_size_tiles(int* out_w, int* out_h) {
    if (out_w) *out_w = g_world.w;
    if (out_h) *out_h = g_world.h;
}

void world_size_px(int* out_w, int* out_h) {
    int ts = g_world.tile_size;
    if (out_w) *out_w = g_world.w * ts;
    if (out_h) *out_h = g_world.h * ts;
}

world_tile_t world_tile_at(int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= g_world.w || ty >= g_world.h || !g_world.tiles) {
        return WORLD_TILE_VOID;
    }
    return g_world.tiles[ty * g_world.w + tx];
}

bool world_is_walkable_px(float x, float y) {
    int ts = g_world.tile_size;
    if (ts <= 0 || g_world.w <= 0 || g_world.h <= 0) return false;

    int tx = (int)floorf(x / (float)ts);
    int ty = (int)floorf(y / (float)ts);
    return world_tile_at(tx, ty) == WORLD_TILE_WALKABLE;
}

v2f world_get_spawn_px(void) {
    if (!g_world.tiles) {
        return v2f_make(0.0f, 0.0f);
    }
    return g_world.spawn;
}
