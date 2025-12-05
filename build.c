#define NOB_IMPLEMENTATION
#include "third_party/nob.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define CHIPMUNK_INCLUDE_DIR   "third_party/Chipmunk2D/include"

#if defined(_WIN32)
    #define CHIPMUNK_LIB_DIR   "third_party/Chipmunk2D/build"
#else
    #define CHIPMUNK_LIB_DIR   "third_party/Chipmunk2D/build/src"
    #define CHIPMUNK_LIB_NAME  "libchipmunk.so.7.0.3"
#endif

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
        "-fno-fast-math", "-fno-finite-math-only", \
        "-I", "include", \
        "-I", CHIPMUNK_INCLUDE_DIR, \
        "src/main.c", \
        "src/modules/*.c", \
        "-o", "build/game.exe", \
        "-L", "lib", \
        "-L", CHIPMUNK_LIB_DIR, \
        "-lraylib", "-lchipmunk", "-lgdi32", "-lwinmm"

#endif /* _WIN32 */

/* ────────────────────────────────────────────── */

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!nob_mkdir_if_not_exists("build")) return 1;

    Nob_Cmd cmd = {0};

#ifdef _WIN32
    // Unified call — no platform branches here
    nob_cmd_append(&cmd, BUILD_CMD_PROG, BUILD_CMD_ARGS);
#else
    const char *chipmunk_lib_dir = CHIPMUNK_LIB_DIR;
    const char *chipmunk_lib_name = CHIPMUNK_LIB_NAME;
    const char *chipmunk_lib_file = nob_temp_sprintf("%s/%s", chipmunk_lib_dir, chipmunk_lib_name);
    nob_log(NOB_INFO, "Linking against %s", chipmunk_lib_file);
    const char *chipmunk_rpath = nob_temp_sprintf("$ORIGIN/../%s", chipmunk_lib_dir);

    char *cmdline = nob_temp_sprintf(
        "gcc -std=c99 -Wall -Wextra -fno-fast-math -fno-finite-math-only "
        "$(pkg-config --cflags raylib) "
        "-I %s "
        "src/main.c src/modules/*.c "
        "-o build/game "
        "$(pkg-config --libs raylib) "
        "-L %s -l:%s "
        "-Wl,-rpath,'%s' -lm",
        CHIPMUNK_INCLUDE_DIR,
        chipmunk_lib_dir,
        chipmunk_lib_name,
        chipmunk_rpath
    );
    nob_cmd_append(&cmd, "sh", "-lc", cmdline);
#endif

    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    if (!sync_assets()) return 1;

    return 0;
}
