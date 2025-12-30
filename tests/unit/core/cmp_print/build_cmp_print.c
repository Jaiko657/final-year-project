#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#define NOB_IMPLEMENTATION
#include "../../../../third_party/nob.h"

#include "../../test_runner/runner_gen.c"

#include <string.h>

static const char *sanitize_path_for_obj(const char *path)
{
    Nob_String_Builder sb = {0};
    for (const char *p = path; p && *p; ++p) {
        char c = *p;
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9'))
        {
            nob_sb_append_buf(&sb, &c, 1);
        } else {
            char u = '_';
            nob_sb_append_buf(&sb, &u, 1);
        }
    }
    nob_sb_append_null(&sb);
    return sb.items;
}

static bool compile_obj(const char *cc, const char *cflags, const char *includes, const char *src, const char *obj)
{
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "sh", "-lc",
        nob_temp_sprintf("%s %s %s -c %s -o %s",
            cc,
            cflags ? cflags : "",
            includes ? includes : "",
            src,
            obj
        )
    );
    return nob_cmd_run_sync_and_reset(&cmd);
}

static void sb_append_paths(Nob_String_Builder *sb, const Nob_File_Paths *paths)
{
    for (size_t i = 0; i < paths->count; ++i) {
        nob_sb_append_cstr(sb, paths->items[i]);
        nob_sb_append_cstr(sb, " ");
    }
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    bool coverage = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--coverage") == 0) coverage = true;
    }

    if (!nob_mkdir_if_not_exists("build/tests")) return 1;
    if (!nob_mkdir_if_not_exists("build/tests/gen")) return 1;
    if (!nob_mkdir_if_not_exists("build/tests/obj")) return 1;
    if (!nob_mkdir_if_not_exists("build/tests/obj/cmp_print")) return 1;
    if (!nob_mkdir_if_not_exists("build/tests/plugins")) return 1;

    Nob_File_Paths test_sources = {0};
    nob_da_append(&test_sources, "tests/unit/core/cmp_print/test_cmp_print.c");

    const char *runner_path = "build/tests/gen/tests_cmp_print_runner.c";
    if (!generate_unity_runner("cmp_print", &test_sources, runner_path)) return 1;

    const char *cc = getenv("CC");
    if (!cc || cc[0] == '\0') cc = "cc";

    const char *includes =
        "-I third_party/Unity/src "
        "-I src "
        ""
        "-I tests/unit/core/cmp_print "
        "-I tests/unit/test_runner";
    const char *cflags = coverage
        ? "-std=c99 -Wall -Wextra -O0 -g -fPIC --coverage "
        : "-std=c99 -Wall -Wextra -O0 -g -fPIC ";

    Nob_File_Paths sources = {0};
    nob_da_append(&sources, "third_party/Unity/src/unity.c");
    nob_da_append(&sources, "src/modules/core/cmp_print.c");
    nob_da_append(&sources, "src/modules/core/logger.c");
    nob_da_append(&sources, "tests/unit/core/cmp_print/test_cmp_print.c");
    nob_da_append(&sources, runner_path);

    Nob_File_Paths objs = {0};
    for (size_t i = 0; i < sources.count; ++i) {
        const char *src = sources.items[i];
        const char *stem = sanitize_path_for_obj(src);
        const char *obj = nob_temp_sprintf("build/tests/obj/cmp_print/%s.o", stem);
        nob_da_append(&objs, obj);
        if (!compile_obj(cc, cflags, includes, src, obj)) return 1;
    }

    Nob_Cmd cmd = {0};
    {
        Nob_String_Builder link = {0};
        nob_sb_appendf(&link, "%s -shared ", cc);
        sb_append_paths(&link, &objs);
        if (coverage) nob_sb_append_cstr(&link, "--coverage ");
        nob_sb_append_cstr(&link, "-o build/tests/plugins/tests_cmp_print.so -lm");
        nob_sb_append_null(&link);
        nob_cmd_append(&cmd, "sh", "-lc", link.items);
        if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;
        nob_sb_free(link);
    }

    return 0;
}
