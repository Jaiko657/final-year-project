#define NOB_IMPLEMENTATION
#include "third_party/nob.h"

static bool sync_assets(void) {
    if (!nob_mkdir_if_not_exists("build")) return false;
    return nob_copy_directory_recursively("assets", "build/assets");
}

#ifdef _WIN32
    /* Windows (w64devkit: manual lib/include from raylib repo) */

    #define BUILD_CMD_PROG      "gcc"

    // Each argument is a separate token — no shell, no quoting issues
    #define BUILD_CMD_ARGS \
        "-std=c99", "-Wall", "-Wextra", \
        "-I", "include", \
        "src/main.c", \
        "src/modules/*.c", \
        "-o", "build/game.exe", \
        "-L", "lib", \
        "-lraylib", "-lgdi32", "-lwinmm"

#else
    /* Linux */

    #define BUILD_CMD_PROG      "sh"

    // Linux needs the shell for globs + pkg-config. Windows does not.
    #define BUILD_CMD_ARGS \
        "-lc", \
        "gcc -std=c99 -Wall -Wextra " \
            "$(pkg-config --cflags raylib) " \
            "src/main.c src/modules/*.c " \
            "-o build/game " \
            "$(pkg-config --libs raylib) -lm"
#endif

/* ────────────────────────────────────────────── */

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!nob_mkdir_if_not_exists("build")) return 1;

    Nob_Cmd cmd = {0};

    // Unified call — no platform branches here
    nob_cmd_append(&cmd, BUILD_CMD_PROG, BUILD_CMD_ARGS);

    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    if (!sync_assets()) return 1;

    return 0;
}