#define NOB_IMPLEMENTATION
#include "third_party/nob.h"

static bool sync_assets(void) {
    if (!nob_mkdir_if_not_exists("build")) return false;
    // Recursively copies and overwrites existing files.
    // (Note: it won't delete stale files already in build/assets.)
    return nob_copy_directory_recursively("assets", "build/assets");
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    Nob_Cmd cmd = {0};

    // mkdir -p build
    nob_cmd_append(&cmd, "mkdir", "-p", "build");
    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;

    // compile app
    nob_cmd_append(
        &cmd,
        "sh", "-lc",
        "cc -std=c99 -Wall -Wextra "
        "$(pkg-config --cflags raylib) "
        "src/main.c src/modules/*.c "
        "-o build/game "
        "$(pkg-config --libs raylib) "
        "-lm"
    );
    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;

    if (!sync_assets()) return 1;
    return 0;
}
