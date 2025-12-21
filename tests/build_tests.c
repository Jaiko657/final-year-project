#define NOB_IMPLEMENTATION
#include "../third_party/nob.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *path;
    const char *name;
} Test_Fn;

static bool cstr_ends_with(const char *s, const char *suffix)
{
    if (!s || !suffix) return false;
    size_t n = strlen(s);
    size_t m = strlen(suffix);
    if (m > n) return false;
    return memcmp(s + (n - m), suffix, m) == 0;
}

static bool cstr_starts_with(const char *s, const char *prefix)
{
    if (!s || !prefix) return false;
    size_t n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

static bool is_dot_entry(const char *name)
{
    return name && (strcmp(name, ".") == 0 || strcmp(name, "..") == 0);
}

static void sort_cstrs(Nob_File_Paths *xs)
{
    if (!xs || xs->count < 2) return;
    for (size_t i = 1; i < xs->count; ++i) {
        const char *key = xs->items[i];
        size_t j = i;
        while (j > 0 && strcmp(xs->items[j - 1], key) > 0) {
            xs->items[j] = xs->items[j - 1];
            --j;
        }
        xs->items[j] = key;
    }
}

static bool gather_unit_test_sources(Nob_File_Paths *out_sources)
{
    Nob_File_Paths children = {0};
    if (!nob_read_entire_dir("tests/unit", &children)) return false;

    for (size_t i = 0; i < children.count; ++i) {
        const char *name = children.items[i];
        if (is_dot_entry(name)) continue;
        if (!cstr_ends_with(name, ".c")) continue;
        if (!cstr_starts_with(name, "test_")) continue;
        nob_da_append(out_sources, nob_temp_sprintf("tests/unit/%s", name));
    }
    sort_cstrs(out_sources);
    return true;
}

static bool gather_stub_sources(Nob_File_Paths *out_sources)
{
    Nob_File_Paths children = {0};
    if (!nob_read_entire_dir("tests/stubs", &children)) return false;

    for (size_t i = 0; i < children.count; ++i) {
        const char *name = children.items[i];
        if (is_dot_entry(name)) continue;
        if (!cstr_ends_with(name, ".c")) continue;
        nob_da_append(out_sources, nob_temp_sprintf("tests/stubs/%s", name));
    }
    sort_cstrs(out_sources);
    return true;
}

static const char *sanitize_path_for_obj(const char *path)
{
    // Convert `src/modules/foo.c` -> `src_modules_foo_c` to avoid nested dirs and collisions.
    // (Simple + stable; good enough for this repo.)
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

static bool read_manifest_sources(const char *manifest_path, Nob_File_Paths *out_sources)
{
    Nob_String_Builder sb = {0};
    if (!nob_read_entire_file(manifest_path, &sb)) return false;
    nob_sb_append_null(&sb);

    char *p = sb.items;
    while (p && *p) {
        char *line = p;
        char *nl = strchr(p, '\n');
        if (nl) {
            *nl = '\0';
            p = nl + 1;
        } else {
            p = NULL;
        }

        // trim leading spaces
        while (*line == ' ' || *line == '\t' || *line == '\r') line++;
        if (*line == '\0') continue;
        if (*line == '#') continue;

        // trim trailing spaces
        char *end = line + strlen(line);
        while (end > line && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
            end[-1] = '\0';
            end--;
        }

        if (*line == '\0') continue;
        nob_da_append(out_sources, nob_temp_strdup(line));
    }

    sort_cstrs(out_sources);
    nob_sb_free(sb);
    return true;
}

static void sort_test_fns(Test_Fn *items, size_t count)
{
    // sort by name for stable ordering
    for (size_t i = 1; i < count; ++i) {
        Test_Fn key = items[i];
        size_t j = i;
        while (j > 0 && strcmp(items[j - 1].name, key.name) > 0) {
            items[j] = items[j - 1];
            --j;
        }
        items[j] = key;
    }
}

static bool extract_test_functions(const char *path, Test_Fn **out_items, size_t *out_count, size_t *out_cap)
{
    Nob_String_Builder sb = {0};
    if (!nob_read_entire_file(path, &sb)) return false;
    nob_sb_append_null(&sb);

    const char *s = sb.items;
    const char *needle = "void test_";
    const size_t needle_len = strlen(needle);

    for (;;) {
        const char *hit = strstr(s, needle);
        if (!hit) break;

        const char *name_start = hit + strlen("void ");
        const char *paren = strchr(name_start, '(');
        if (!paren) break;

        // Extract identifier
        size_t name_len = (size_t)(paren - name_start);
        if (name_len > 0 && name_len < 256) {
            char buf[256];
            memcpy(buf, name_start, name_len);
            buf[name_len] = '\0';

            // Only treat `void test_foo(void)` as a Unity test entrypoint.
            const char *args = paren + 1;
            while (*args == ' ' || *args == '\t' || *args == '\r' || *args == '\n') args++;
            bool args_ok = (strncmp(args, "void", 4) == 0);
            if (args_ok) {
                args += 4;
                while (*args == ' ' || *args == '\t' || *args == '\r' || *args == '\n') args++;
                args_ok = (*args == ')');
            }

            // Basic validation: must start with test_ and contain only identifier chars
            bool ok = args_ok && cstr_starts_with(buf, "test_");
            for (size_t i = 0; ok && i < name_len; ++i) {
                char c = buf[i];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) ok = false;
            }

            if (ok) {
                if (*out_count >= *out_cap) {
                    size_t new_cap = (*out_cap == 0) ? 64 : (*out_cap * 2);
                    *out_items = (Test_Fn*)NOB_REALLOC(*out_items, new_cap * sizeof(**out_items));
                    NOB_ASSERT(*out_items);
                    *out_cap = new_cap;
                }
                (*out_items)[(*out_count)++] = (Test_Fn){ .path = path, .name = nob_temp_strdup(buf) };
            }
        }

        s = hit + needle_len;
    }

    nob_sb_free(sb);
    return true;
}

static bool generate_unity_runner(const Nob_File_Paths *test_sources, const char *out_path)
{
    Test_Fn *items = NULL;
    size_t count = 0;
    size_t cap = 0;

    for (size_t i = 0; i < test_sources->count; ++i) {
        if (!extract_test_functions(test_sources->items[i], &items, &count, &cap)) {
            NOB_FREE(items);
            return false;
        }
    }

    if (count == 0) {
        nob_log(NOB_ERROR, "No test_* functions found in tests/unit/test_*.c");
        NOB_FREE(items);
        return false;
    }

    sort_test_fns(items, count);

    Nob_String_Builder sb = {0};
    nob_sb_append_cstr(&sb, "#include \"unity.h\"\n\n");
    for (size_t i = 0; i < count; ++i) {
        nob_sb_appendf(&sb, "void %s(void);\n", items[i].name);
    }
    nob_sb_append_cstr(&sb, "\nvoid setUp(void) {}\nvoid tearDown(void) {}\n\n");
    nob_sb_append_cstr(&sb, "int main(void)\n{\n    UNITY_BEGIN();\n\n");
    for (size_t i = 0; i < count; ++i) {
        nob_sb_appendf(&sb, "    RUN_TEST(%s);\n", items[i].name);
    }
    nob_sb_append_cstr(&sb, "\n    return UNITY_END();\n}\n");

    if (!nob_write_entire_file(out_path, sb.items, sb.count)) {
        nob_sb_free(sb);
        NOB_FREE(items);
        return false;
    }

    nob_sb_free(sb);
    NOB_FREE(items);
    return true;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    bool coverage = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--coverage") == 0) coverage = true;
    }

    if (!nob_mkdir_if_not_exists("build")) return 1;
    if (!nob_mkdir_if_not_exists("build/obj")) return 1;
    if (!nob_mkdir_if_not_exists("build/obj/unit")) return 1;
    if (!nob_mkdir_if_not_exists("build/obj/unit/gen")) return 1;
    if (coverage) {
        if (!nob_mkdir_if_not_exists("build/coverage")) return 1;
        // Avoid gcov checksum warnings when toggling sources/flags between runs.
        Nob_Cmd clean = {0};
        nob_cmd_append(&clean, "sh", "-lc", "rm -f build/obj/unit/*.gcda build/obj/unit/*.gcov");
        if (!nob_cmd_run_sync_and_reset(&clean)) return 1;
    }

    Nob_File_Paths test_sources = {0};
    if (!gather_unit_test_sources(&test_sources)) return 1;

    Nob_File_Paths stub_sources = {0};
    if (!gather_stub_sources(&stub_sources)) return 1;

    Nob_File_Paths manifest_sources = {0};
    if (!read_manifest_sources("tests/unit/manifest.txt", &manifest_sources)) return 1;

    const char *runner_path = "build/obj/unit/gen/unity_runner.c";
    if (!generate_unity_runner(&test_sources, runner_path)) return 1;

    const char *cc = getenv("CC");
    if (!cc || cc[0] == '\0') cc = "cc";

    const char *includes =
        "-I third_party/Unity/src "
        "-I src/includes "
        "-I third_party/xml.c/src "
        "-I third_party/Chipmunk2D/include "
        "-I tests/stubs";
    const char *cflags = coverage
        ? "-std=c99 -Wall -Wextra -O0 -g --coverage"
        : "-std=c99 -Wall -Wextra -O0 -g";

    // Compile all sources to objects (keeps coverage artifacts in build/obj/unit).
    Nob_File_Paths all_sources = {0};
    nob_da_append(&all_sources, "third_party/Unity/src/unity.c");
    for (size_t i = 0; i < manifest_sources.count; ++i) nob_da_append(&all_sources, manifest_sources.items[i]);
    for (size_t i = 0; i < stub_sources.count; ++i) nob_da_append(&all_sources, stub_sources.items[i]);
    for (size_t i = 0; i < test_sources.count; ++i) nob_da_append(&all_sources, test_sources.items[i]);
    nob_da_append(&all_sources, runner_path);

    Nob_File_Paths objs = {0};
    for (size_t i = 0; i < all_sources.count; ++i) {
        const char *src = all_sources.items[i];
        const char *stem = sanitize_path_for_obj(src);
        const char *obj = nob_temp_sprintf("build/obj/unit/%s.o", stem);
        nob_da_append(&objs, obj);
        const char *file_cflags = cstr_starts_with(src, "third_party/")
            ? nob_temp_sprintf("%s -w", cflags)
            : cflags;
        if (!compile_obj(cc, file_cflags, includes, src, obj)) return 1;
    }

    Nob_Cmd cmd = {0};
    {
        Nob_String_Builder link = {0};
        nob_sb_appendf(&link, "%s %s ", cc, cflags);
        sb_append_paths(&link, &objs);
        nob_sb_append_cstr(&link, "-o build/unit_tests");
        nob_sb_append_null(&link);
        nob_cmd_append(&cmd, "sh", "-lc", link.items);
        if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;
        nob_sb_free(link);
    }

    nob_cmd_append(&cmd, "./build/unit_tests");
    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;

    if (coverage) {
        // HTML report if lcov is available, otherwise leave raw .gcda/.gcno in build/obj/unit.
        const char *cover_cmd =
            "set -euo pipefail; "
            "exec 2> >(grep -v 'Duplicate specification' >&2); "
            "if command -v lcov >/dev/null 2>&1 && command -v genhtml >/dev/null 2>&1; then "
            "  lcov --capture --directory build/obj/unit --output-file build/coverage/lcov.info >/dev/null; "
            "  lcov --remove build/coverage/lcov.info '*/third_party/*' '*/tests/*' --output-file build/coverage/lcov.info.cleaned >/dev/null; "
            "  genhtml build/coverage/lcov.info.cleaned --output-directory build/coverage/html >/dev/null; "
            "  echo \"[coverage] HTML: build/coverage/html/index.html\"; "
            "else "
            "  echo \"[coverage] Install lcov+genhtml to generate HTML (coverage data is in build/obj/unit).\"; "
            "fi";
        nob_cmd_append(&cmd, "bash", "-lc", cover_cmd);
        if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    }

    return 0;
}
