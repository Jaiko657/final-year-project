#include "../includes/world_physics.h"
#include "../includes/world.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

static cpSpace* g_space = NULL;
static cpShape** g_tile_shapes = NULL;
static size_t g_tile_shape_count = 0;

static void destroy_world_tiles(void)
{
    if (!g_tile_shapes) return;
    for (size_t i = 0; i < g_tile_shape_count; ++i) {
        cpShape* shape = g_tile_shapes[i];
        if (!shape) continue;
        if (g_space) {
            cpSpaceRemoveShape(g_space, shape);
        }
        cpShapeFree(shape);
    }
    free(g_tile_shapes);
    g_tile_shapes = NULL;
    g_tile_shape_count = 0;
}

static void create_world_tiles(void)
{
    if (!g_space) return;
    destroy_world_tiles();

    int tiles_w = 0, tiles_h = 0;
    world_size_tiles(&tiles_w, &tiles_h);
    const int tile_px = world_tile_size();
    const int subtile_px = world_subtile_size();
    if (tiles_w <= 0 || tiles_h <= 0 || tile_px <= 0 || subtile_px <= 0) return;

    int subtiles_per_tile = tile_px / subtile_px;
    if (subtiles_per_tile <= 0) return;

    size_t capacity = (size_t)tiles_w * (size_t)tiles_h * (size_t)(subtiles_per_tile * subtiles_per_tile);
    if (capacity == 0) return;

    g_tile_shapes = (cpShape**)calloc(capacity, sizeof(cpShape*));
    if (!g_tile_shapes) return;

    size_t cell_count = (size_t)subtiles_per_tile * (size_t)subtiles_per_tile;
    bool* tile_solid = (bool*)malloc(cell_count * sizeof(bool));
    if (!tile_solid) {
        free(g_tile_shapes);
        g_tile_shapes = NULL;
        return;
    }

    cpBody* static_body = cpSpaceGetStaticBody(g_space);
    const cpFloat subtile_size = (cpFloat)subtile_px;

    for (int ty = 0; ty < tiles_h; ++ty) {
        for (int tx = 0; tx < tiles_w; ++tx) {
            if (world_tile_is_dynamic(tx, ty)) {
                // Dynamic colliders (e.g., doors) are handled live elsewhere; skip cached merge.
                continue;
            }
            int remaining = 0;
            for (size_t i = 0; i < cell_count; ++i) tile_solid[i] = false;

            // Gather solid subtiles for this tile only.
            for (int sy = 0; sy < subtiles_per_tile; ++sy) {
                for (int sx = 0; sx < subtiles_per_tile; ++sx) {
                    int global_sx = tx * subtiles_per_tile + sx;
                    int global_sy = ty * subtiles_per_tile + sy;
                    bool solid = !world_is_walkable_subtile(global_sx, global_sy);
                    tile_solid[(size_t)sy * (size_t)subtiles_per_tile + (size_t)sx] = solid;
                    if (solid) ++remaining;
                }
            }

            while (remaining > 0) {
                int best_area = 0;
                int best_x0 = 0, best_y0 = 0, best_x1 = 0, best_y1 = 0;

                // Find the largest solid rectangle within this tile.
                for (int y0 = 0; y0 < subtiles_per_tile; ++y0) {
                    for (int x0 = 0; x0 < subtiles_per_tile; ++x0) {
                        if (!tile_solid[(size_t)y0 * (size_t)subtiles_per_tile + (size_t)x0]) continue;
                        for (int y1 = y0; y1 < subtiles_per_tile; ++y1) {
                            for (int x1 = x0; x1 < subtiles_per_tile; ++x1) {
                                int area = (x1 - x0 + 1) * (y1 - y0 + 1);
                                if (area <= best_area) continue;

                                bool full = true;
                                for (int yy = y0; yy <= y1 && full; ++yy) {
                                    for (int xx = x0; xx <= x1; ++xx) {
                                        if (!tile_solid[(size_t)yy * (size_t)subtiles_per_tile + (size_t)xx]) {
                                            full = false;
                                            break;
                                        }
                                    }
                                }

                                if (full) {
                                    best_area = area;
                                    best_x0 = x0; best_y0 = y0;
                                    best_x1 = x1; best_y1 = y1;
                                }
                            }
                        }
                    }
                }

                if (best_area <= 0) break;

                const cpFloat left   = (cpFloat)(tx * subtiles_per_tile + best_x0) * subtile_size;
                const cpFloat bottom = (cpFloat)(ty * subtiles_per_tile + best_y0) * subtile_size;
                const cpFloat right  = (cpFloat)(tx * subtiles_per_tile + best_x1 + 1) * subtile_size;
                const cpFloat top    = (cpFloat)(ty * subtiles_per_tile + best_y1 + 1) * subtile_size;

                cpShape* shape = cpBoxShapeNew2(static_body, cpBBNew(left, bottom, right, top), 0.0f);
                if (shape) {
                    cpShapeSetFriction(shape, 1.0f);
                    cpShapeSetElasticity(shape, 0.0f);
                    cpSpaceAddShape(g_space, shape);
                    if (g_tile_shape_count < capacity) {
                        g_tile_shapes[g_tile_shape_count++] = shape;
                    } else {
                        cpSpaceRemoveShape(g_space, shape);
                        cpShapeFree(shape);
                    }
                }

                // Clear the cells we just consumed.
                for (int y = best_y0; y <= best_y1; ++y) {
                    for (int x = best_x0; x <= best_x1; ++x) {
                        size_t idx = (size_t)y * (size_t)subtiles_per_tile + (size_t)x;
                        if (tile_solid[idx]) {
                            tile_solid[idx] = false;
                            --remaining;
                        }
                    }
                }
            }
        }
    }

    free(tile_solid);

    if (g_tile_shape_count == 0) {
        free(g_tile_shapes);
        g_tile_shapes = NULL;
    }
}

static void count_body_cb(cpBody* body, void* data)
{
    (void)body;
    int* b = (int*)data;
    (*b)++;
}

static void count_shape_cb(cpShape* shape, void* data)
{
    (void)shape;
    int* s = (int*)data;
    (*s)++;
}

bool world_physics_ready(void)
{
    return g_space != NULL;
}

void world_physics_init(void)
{
    if (g_space) return;
    g_space = cpSpaceNew();
    cpSpaceSetGravity(g_space, cpv(0.0f, 0.0f));
// --- Very strict collision settings ---       TODO: Tweak these if needing more performance
    cpSpaceSetIterations(g_space, 30);                              // more solver passes
    cpSpaceSetCollisionSlop(g_space, 0.01f);                        // almost no allowed overlap
    cpSpaceSetCollisionBias(g_space, cpfpow(1.0f - 0.2f, 60.0f));   // (default is already fine, but this is a bit more aggressive)


    create_world_tiles();
}

void world_physics_rebuild_static(void)
{
    if (!g_space) return;
    destroy_world_tiles();
    create_world_tiles();
}

void world_physics_shutdown(void)
{
    if (g_space) {
        destroy_world_tiles();
        cpSpaceFree(g_space);
        g_space = NULL;
    }
}

bool world_physics_add_body(cpBody* body)
{
    if (!g_space || !body) return false;
    cpSpaceAddBody(g_space, body);
    return true;
}

bool world_physics_add_shape(cpShape* shape)
{
    if (!g_space || !shape) return false;
    cpSpaceAddShape(g_space, shape);
    return true;
}

void world_physics_remove_body(cpBody* body)
{
    if (!g_space || !body) return;
    cpSpaceRemoveBody(g_space, body);
}

void world_physics_remove_shape(cpShape* shape)
{
    if (!g_space || !shape) return;
    cpSpaceRemoveShape(g_space, shape);
}

world_physics_counts_t world_physics_counts(void)
{
    world_physics_counts_t c = {0, 0};
    if (!g_space) return c;
    cpSpaceEachBody(g_space, count_body_cb, &c.bodies);
    cpSpaceEachShape(g_space, count_shape_cb, &c.shapes);
    return c;
}

void world_physics_step(float dt)
{
    if (!g_space) return;
    cpSpaceStep(g_space, dt);
}

void world_physics_for_each_static_shape(void (*cb)(const cpBB* bb, void* ud), void* ud)
{
    if (!cb || !g_tile_shapes) return;
    for (size_t i = 0; i < g_tile_shape_count; ++i) {
        cpShape* shape = g_tile_shapes[i];
        if (!shape) continue;
        const cpBB bb = cpShapeGetBB(shape);
        cb(&bb, ud);
    }
}
