#include "raylib.h"
#include <stdio.h>
#include "tiled_parser.h"
#include "tiled_renderer.h"

int main(void) {
    tiled_map_t map;
    if (!tiled_load_map("../start.tmx", &map)) {
        fprintf(stderr, "Unable to load TMX\n");
        return 1;
    }

    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "TMX parsing demo");
    if (!IsWindowReady()) {
        fprintf(stderr, "Failed to init window\n");
        tiled_free_map(&map);
        return 1;
    }

    tiled_renderer_t renderer;
    if (!tiled_renderer_init(&renderer, &map)) {
        tiled_free_map(&map);
        CloseWindow();
        return 1;
    }

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        tiled_renderer_draw(&map, &renderer);
        DrawText("TMX/TSX parsing PoC (start.tmx)", 10, 10, 20, DARKGRAY);
        EndDrawing();
    }

    tiled_renderer_shutdown(&renderer);
    tiled_free_map(&map);
    CloseWindow();
    return 0;
}
