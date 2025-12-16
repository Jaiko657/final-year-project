#define NOB_IMPLEMENTATION
#include "third_party/nob.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define CHIPMUNK_INCLUDE_DIR   "third_party/Chipmunk2D/include"

#define RAYLIB_INCLUDE_DIR     "third_party/raylib/src"

#define XML_INCLUDE_DIR        "third_party/xml.c/src"

#if defined(_WIN32)
    #define CHIPMUNK_LIB_DIR   "third_party/Chipmunk2D/build"
#else
    #define CHIPMUNK_LIB_DIR   "third_party/Chipmunk2D/build/src"
    #define CHIPMUNK_LIB_NAME  "libchipmunk.so.7.0.3"
#endif

#define XML_LIB_DIR            "third_party/xml.c/build"
#define XML_LIB_NAME           "libxml.a"

static bool sync_assets(void) {
    return true;
    if (!nob_mkdir_if_not_exists("build")) return false;
    return nob_copy_directory_recursively("assets", "build/assets");
}

/* ────────────────────────────────────────────── */

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    bool debug_build = false;
    for (int i = 1; i < argc; ++i) {
        if (nob_sv_eq(nob_sv_from_cstr(argv[i]), nob_sv_from_cstr("--debug"))) debug_build = true;
        if (nob_sv_eq(nob_sv_from_cstr(argv[i]), nob_sv_from_cstr("--release"))) debug_build = false;
    }

    if (!nob_mkdir_if_not_exists("build")) return 1;
    nob_log(NOB_INFO, "Mode: %s", debug_build ? "debug" : "release");

    Nob_Cmd cmd = {0};

    const char *raylib_include_dir = RAYLIB_INCLUDE_DIR;
    const char *raylib_lib_file = NULL;

#ifdef _WIN32
    const char *raylib_candidates[] = {
        "third_party/raylib/build/raylib/libraylib.a",
        "third_party/raylib/build/raylib/raylib.lib",
        "third_party/raylib/build/raylib/Release/raylib.lib",
        "third_party/raylib/build/raylib/Debug/raylib.lib",
    };
#else
    const char *raylib_candidates[] = {
        "third_party/raylib/build/raylib/libraylib.a",
        "third_party/raylib/build/raylib/libraylib.so",
    };
#endif

    for (size_t i = 0; i < sizeof(raylib_candidates)/sizeof(raylib_candidates[0]); ++i) {
        if (nob_file_exists(raylib_candidates[i])) {
            raylib_lib_file = raylib_candidates[i];
            break;
        }
    }

    if (raylib_lib_file == NULL) {
        nob_log(NOB_ERROR, "Could not find raylib build output. Run third_party/setup.sh first.");
        return 1;
    }
    nob_log(NOB_INFO, "Linking against %s", raylib_lib_file);

#ifdef _WIN32
    nob_cmd_append(&cmd, "gcc",
        "-std=c99", "-Wall", "-Wextra",
        "-fno-fast-math", "-fno-finite-math-only",
        debug_build ? "-DDEBUG_BUILD=1" : "-DDEBUG_BUILD=0",
        debug_build ? "-DDEBUG_COLLISION=1" : "-DDEBUG_COLLISION=0",
        debug_build ? "-DDEBUG_TRIGGERS=1" : "-DDEBUG_TRIGGERS=0",
        debug_build ? "-DDEBUG_FPS=1" : "-DDEBUG_FPS=0",
        debug_build ? "-g" : "-DNDEBUG",
        "-I", "include",
        "-I", raylib_include_dir,
        "-I", CHIPMUNK_INCLUDE_DIR,
        "-I", XML_INCLUDE_DIR,
        "src/main.c",
        "src/modules/*.c",
        "-o", "build/game.exe",
        "-L", "lib",
        "-L", CHIPMUNK_LIB_DIR,
        "-L", XML_LIB_DIR,
        raylib_lib_file,
        "-lchipmunk", "-l:libxml.a",
        "-lopengl32", "-lgdi32", "-lwinmm"
    );
#else
    const char *debug_flags = debug_build
        ? "-DDEBUG_BUILD=1 -DDEBUG_COLLISION=1 -DDEBUG_TRIGGERS=1 -DDEBUG_FPS=1 -g"
        : "-DDEBUG_BUILD=0 -DDEBUG_COLLISION=0 -DDEBUG_TRIGGERS=0 -DDEBUG_FPS=0 -DNDEBUG";

    const char *chipmunk_lib_dir = CHIPMUNK_LIB_DIR;
    const char *chipmunk_lib_name = CHIPMUNK_LIB_NAME;
    const char *chipmunk_lib_file = nob_temp_sprintf("%s/%s", chipmunk_lib_dir, chipmunk_lib_name);
    nob_log(NOB_INFO, "Linking against %s", chipmunk_lib_file);
    const char *xml_lib_dir = XML_LIB_DIR;
    const char *xml_lib_name = XML_LIB_NAME;
    const char *xml_lib_file = nob_temp_sprintf("%s/%s", xml_lib_dir, xml_lib_name);
    nob_log(NOB_INFO, "Linking against %s", xml_lib_file);
    const char *chipmunk_rpath = nob_temp_sprintf("$ORIGIN/../%s", chipmunk_lib_dir);

    char *cmdline = nob_temp_sprintf(
        "gcc -std=c99 -Wall -Wextra -fno-fast-math -fno-finite-math-only "
        "%s "
        "-I %s -I %s -I %s "
        "src/main.c src/modules/*.c "
        "-o build/game "
        "%s "
        "-L %s -l:%s "
        "-L %s -l:%s "
        "-Wl,-rpath,'%s' "
        "-lGL -lm -lpthread -ldl -lrt -lX11",
        debug_flags,
        raylib_include_dir,
        CHIPMUNK_INCLUDE_DIR,
        XML_INCLUDE_DIR,
        raylib_lib_file,
        chipmunk_lib_dir,
        chipmunk_lib_name,
        xml_lib_dir,
        xml_lib_name,
        chipmunk_rpath
    );
    nob_cmd_append(&cmd, "sh", "-lc", cmdline);
#endif

    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    if (!sync_assets()) return 1;

    return 0;
}
