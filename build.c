#define NOB_IMPLEMENTATION
#include "third_party/nob.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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
	    return true; // I realised i can set working directory but left this as im not sold yet
	    if (!nob_mkdir_if_not_exists("build")) return false;
	    return nob_copy_directory_recursively("assets", "build/assets");
}

static bool cstr_ends_with(const char *s, const char *suffix)
{
    if (!s || !suffix) return false;
    size_t n = strlen(s);
    size_t m = strlen(suffix);
    if (m > n) return false;
    return memcmp(s + (n - m), suffix, m) == 0;
}

static bool is_dot_entry(const char *name)
{
    return name && (strcmp(name, ".") == 0 || strcmp(name, "..") == 0);
}

static bool list_contains_cstr(const Nob_File_Paths *xs, const char *s)
{
    if (!xs || !s) return false;
    for (size_t i = 0; i < xs->count; ++i) {
        if (xs->items[i] && strcmp(xs->items[i], s) == 0) return true;
    }
    return false;
}

static bool gather_headless_sources(Nob_File_Paths *out_sources, Nob_File_Paths *out_replacement_bases)
{
    Nob_File_Paths children = {0};
    if (!nob_read_entire_dir("src/headless", &children)) return false;

    for (size_t i = 0; i < children.count; ++i) {
        const char *name = children.items[i];
        if (is_dot_entry(name)) continue;
        if (!cstr_ends_with(name, ".c")) continue;

        const char *full = nob_temp_sprintf("src/headless/%s", name);
        nob_da_append(out_sources, full);

        // If a file follows `*_headless.c`, treat it as an override for `src/modules/<base>.c`.
        if (cstr_ends_with(name, "_headless.c")) {
            size_t n = strlen(name);
            size_t suffix = strlen("_headless.c");
            size_t base_len = (n > suffix) ? (n - suffix) : 0;
            if (base_len == 0) continue;
            const char *base = nob_temp_sprintf("%.*s", (int)base_len, name);
            if (!list_contains_cstr(out_replacement_bases, base)) {
                nob_da_append(out_replacement_bases, base);
            }
        }
    }

    return true;
}

static bool gather_module_sources(Nob_File_Paths *out_sources, const Nob_File_Paths *replacement_bases)
{
    Nob_File_Paths children = {0};
    if (!nob_read_entire_dir("src/modules", &children)) return false;

    for (size_t i = 0; i < children.count; ++i) {
        const char *name = children.items[i];
        if (is_dot_entry(name)) continue;
        if (!cstr_ends_with(name, ".c")) continue;

        // Skip module sources that have a headless replacement.
        size_t n = strlen(name);
        size_t ext = strlen(".c");
        size_t base_len = (n > ext) ? (n - ext) : 0;
        const char *base = (base_len > 0) ? nob_temp_sprintf("%.*s", (int)base_len, name) : "";
        if (replacement_bases && list_contains_cstr(replacement_bases, base)) continue;

        const char *full = nob_temp_sprintf("src/modules/%s", name);
        nob_da_append(out_sources, full);
    }

    return true;
}

static void sb_append_paths(Nob_String_Builder *sb, const Nob_File_Paths *paths)
{
    for (size_t i = 0; i < paths->count; ++i) {
        nob_sb_append_cstr(sb, paths->items[i]);
        nob_sb_append_cstr(sb, " ");
    }
}

int main(int argc, char **argv) {
	    NOB_GO_REBUILD_URSELF(argc, argv);

	    bool debug_build = false;
	    bool headless_build = false;
    for (int i = 1; i < argc; ++i) {
        if (nob_sv_eq(nob_sv_from_cstr(argv[i]), nob_sv_from_cstr("--debug"))) debug_build = true;
        if (nob_sv_eq(nob_sv_from_cstr(argv[i]), nob_sv_from_cstr("--release"))) debug_build = false;
        if (nob_sv_eq(nob_sv_from_cstr(argv[i]), nob_sv_from_cstr("--headless"))) headless_build = true;
    }
    if (headless_build) debug_build = true; // simplify: headless is always debug

    if (!nob_mkdir_if_not_exists("build")) return 1;
    nob_log(NOB_INFO, "Mode: %s%s", debug_build ? "debug" : "release", headless_build ? " (headless)" : "");

    Nob_Cmd cmd = {0};

#if !defined(_WIN32)
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
#endif

#if !defined(_WIN32)
	    if (headless_build) {
	        Nob_File_Paths headless_sources = {0};
	        Nob_File_Paths replacement_bases = {0};
	        Nob_File_Paths module_sources = {0};
	        if (!gather_headless_sources(&headless_sources, &replacement_bases)) return 1;
	        if (!gather_module_sources(&module_sources, &replacement_bases)) return 1;

	        Nob_String_Builder sb = {0};
	        nob_sb_appendf(&sb,
	            "gcc -std=c99 -Wall -Wextra -fno-fast-math -fno-finite-math-only "
	            "%s -DHEADLESS=1 "
	            "-I %s -I %s "
	            "src/main.c ",
	            debug_flags,
	            CHIPMUNK_INCLUDE_DIR,
	            XML_INCLUDE_DIR
	        );
	        sb_append_paths(&sb, &module_sources);
	        sb_append_paths(&sb, &headless_sources);
	        nob_sb_appendf(&sb,
	            "-o build/game_headless "
	            "-L %s -l:%s "
	            "-L %s -l:%s "
	            "-Wl,-rpath,'%s' "
	            "-lm -lpthread -ldl -lrt",
	            chipmunk_lib_dir,
	            chipmunk_lib_name,
	            xml_lib_dir,
	            xml_lib_name,
	            chipmunk_rpath
	        );
	        nob_sb_append_null(&sb);
	        char *cmdline = sb.items;
	        nob_cmd_append(&cmd, "sh", "-lc", cmdline);

	        if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;
	        if (!sync_assets()) return 1;
	        nob_sb_free(sb);
	        return 0;
	    }
#endif

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
	    Nob_File_Paths module_sources = {0};
	    if (!gather_module_sources(&module_sources, NULL)) return 1;

	    Nob_String_Builder sb = {0};
	    nob_sb_appendf(&sb,
	        "gcc -std=c99 -Wall -Wextra -fno-fast-math -fno-finite-math-only "
	        "%s "
	        "-I %s -I %s -I %s "
	        "src/main.c ",
	        debug_flags,
	        raylib_include_dir,
	        CHIPMUNK_INCLUDE_DIR,
	        XML_INCLUDE_DIR
	    );
	    sb_append_paths(&sb, &module_sources);
	    nob_sb_appendf(&sb,
	        "-o build/game "
	        "%s "
	        "-L %s -l:%s "
	        "-L %s -l:%s "
	        "-Wl,-rpath,'%s' "
	        "-lGL -lm -lpthread -ldl -lrt -lX11",
	        raylib_lib_file,
	        chipmunk_lib_dir,
	        chipmunk_lib_name,
	        xml_lib_dir,
	        xml_lib_name,
	        chipmunk_rpath
	    );
	    nob_sb_append_null(&sb);
	    char *cmdline = sb.items;
	    nob_cmd_append(&cmd, "sh", "-lc", cmdline);
#endif

	    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;
	    if (!sync_assets()) return 1;

    return 0;
}
