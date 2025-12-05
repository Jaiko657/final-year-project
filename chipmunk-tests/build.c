#define NOB_IMPLEMENTATION
#include "../third_party/nob.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define CHIPMUNK_INCLUDE_DIR   "../third_party/Chipmunk2D/include"
#define CHIPMUNK_LIB_DIR       "../third_party/Chipmunk2D/build/src"
#define CHIPMUNK_LIB_NAME      "libchipmunk.so.7.0.3"

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!nob_mkdir_if_not_exists("build")) return 1;

    const bool debug = getenv("DEBUG") != NULL;
    const char *debug_flag = debug ? "-g -O0 " : "";
    const char *safe_math_flags = "-fno-fast-math -fno-finite-math-only ";

    const char *chipmunk_lib_dir = CHIPMUNK_LIB_DIR;
    const char *chipmunk_lib_name = CHIPMUNK_LIB_NAME;
    const char *chipmunk_lib_file = nob_temp_sprintf("%s/%s", chipmunk_lib_dir, chipmunk_lib_name);
    nob_log(NOB_INFO, "Linking against %s", chipmunk_lib_file);
    const char *chipmunk_rpath = nob_temp_sprintf("$ORIGIN/../%s", chipmunk_lib_dir);

    Nob_Cmd cmd = {0};
    char *cmdline = nob_temp_sprintf(
        "gcc -std=c99 -Wall -Wextra %s%s"
        "-I %s "
        "static_demo.c "
        "-o build/static_demo "
        "-L %s -l:%s "
        "-Wl,-rpath,'%s' "
        "-lm",
        debug_flag,
        safe_math_flags,
        CHIPMUNK_INCLUDE_DIR,
        chipmunk_lib_dir,
        chipmunk_lib_name,
        chipmunk_rpath
    );

    nob_cmd_append(&cmd, "sh", "-lc", cmdline);

    return nob_cmd_run_sync_and_reset(&cmd) ? 0 : 1;
}
