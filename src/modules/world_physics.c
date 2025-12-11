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

    cpBody* static_body = cpSpaceGetStaticBody(g_space);
    const cpFloat subtile_size = (cpFloat)subtile_px;

    for (int ty = 0; ty < tiles_h; ++ty) {
        for (int tx = 0; tx < tiles_w; ++tx) {
            for (int sy = 0; sy < subtiles_per_tile; ++sy) {
                for (int sx = 0; sx < subtiles_per_tile; ++sx) {
                    int global_sx = tx * subtiles_per_tile + sx;
                    int global_sy = ty * subtiles_per_tile + sy;
                    if (world_is_walkable_subtile(global_sx, global_sy)) continue;

                    const cpFloat left   = (cpFloat)global_sx * subtile_size;
                    const cpFloat bottom = (cpFloat)global_sy * subtile_size;
                    const cpFloat right  = left + subtile_size;
                    const cpFloat top    = bottom + subtile_size;

                    cpShape* shape = cpBoxShapeNew2(static_body, cpBBNew(left, bottom, right, top), 0.0f);
                    if (!shape) continue;

                    cpShapeSetFriction(shape, 1.0f);
                    cpShapeSetElasticity(shape, 0.0f);
                    cpSpaceAddShape(g_space, shape);
                    g_tile_shapes[g_tile_shape_count++] = shape;
                }
            }
        }
    }

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
