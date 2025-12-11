#define NOB_IMPLEMENTATION
#include "../third_party/nob.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define XML_INCLUDE_DIR "../third_party/xml.c/src"
#define XML_LIB_DIR     "../third_party/xml.c/build"
#define XML_LIB_NAME    "libxml.a"

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!nob_mkdir_if_not_exists("build")) return 1;

    Nob_Cmd cmd = {0};

#ifdef _WIN32
    nob_cmd_append(
        &cmd,
        "gcc",
        "-std=c99", "-Wall", "-Wextra",
        "-fno-fast-math", "-fno-finite-math-only",
        "-I", XML_INCLUDE_DIR,
        "main.c",
        "tiled_parser.c",
        "tiled_renderer.c",
        "-o", "build/tiledparsing_tests.exe",
        "-L", XML_LIB_DIR,
        "-lraylib", "-l:libxml.a", "-lgdi32", "-lwinmm"
    );
#else
    const char *xml_lib_dir = XML_LIB_DIR;
    const char *xml_lib_name = XML_LIB_NAME;
    const char *xml_lib_file = nob_temp_sprintf("%s/%s", xml_lib_dir, xml_lib_name);
    nob_log(NOB_INFO, "Linking against %s", xml_lib_file);

    char *cmdline = nob_temp_sprintf(
        "gcc -std=c99 -Wall -Wextra -fno-fast-math -fno-finite-math-only "
        "$(pkg-config --cflags raylib) "
        "-I %s "
        "main.c tiled_parser.c tiled_renderer.c "
        "-o build/tiledparsing_tests "
        "$(pkg-config --libs raylib) "
        "-L %s -l:%s "
        "-lm",
        XML_INCLUDE_DIR,
        xml_lib_dir,
        xml_lib_name
    );
    nob_cmd_append(&cmd, "sh", "-lc", cmdline);
#endif

    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;

    return 0;
}
