#include "runner_gen.h"

#include <string.h>

typedef struct {
    const char *path;
    const char *name;
} Test_Fn;

static bool cstr_starts_with(const char *s, const char *prefix)
{
    if (!s || !prefix) return false;
    size_t n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

static void sort_test_fns(Test_Fn *items, size_t count)
{
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

        size_t name_len = (size_t)(paren - name_start);
        if (name_len > 0 && name_len < 256) {
            char buf[256];
            memcpy(buf, name_start, name_len);
            buf[name_len] = '\0';

            const char *args = paren + 1;
            while (*args == ' ' || *args == '\t' || *args == '\r' || *args == '\n') args++;
            bool args_ok = (strncmp(args, "void", 4) == 0);
            if (args_ok) {
                args += 4;
                while (*args == ' ' || *args == '\t' || *args == '\r' || *args == '\n') args++;
                args_ok = (*args == ')');
            }

            bool ok = args_ok && cstr_starts_with(buf, "test_");
            for (size_t i = 0; ok && i < name_len; ++i) {
                char c = buf[i];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') || c == '_')) {
                    ok = false;
                }
            }

            if (ok) {
                if (*out_count >= *out_cap) {
                    size_t new_cap = (*out_cap == 0) ? 64 : (*out_cap * 2);
                    *out_items = (Test_Fn *)NOB_REALLOC(*out_items, new_cap * sizeof(**out_items));
                    NOB_ASSERT(*out_items);
                    *out_cap = new_cap;
                }
                (*out_items)[(*out_count)++] = (Test_Fn){
                    .path = path,
                    .name = nob_temp_strdup(buf),
                };
            }
        }

        s = hit + needle_len;
    }

    nob_sb_free(sb);
    return true;
}

bool generate_unity_runner(const char *group, const Nob_File_Paths *test_sources, const char *out_path)
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
        nob_log(NOB_ERROR, "No test_* functions found for group '%s'", group);
        NOB_FREE(items);
        return false;
    }

    sort_test_fns(items, count);

    Nob_String_Builder sb = {0};
    nob_sb_append_cstr(&sb, "#include \"unity.h\"\n");
    nob_sb_append_cstr(&sb, "#include \"plugin_api.h\"\n");
    nob_sb_append_cstr(&sb, "#include <string.h>\n\n");
    nob_sb_append_cstr(&sb, "#if defined(__GNUC__)\n");
    nob_sb_append_cstr(&sb, "__attribute__((weak)) void setUp(void) {}\n");
    nob_sb_append_cstr(&sb, "__attribute__((weak)) void tearDown(void) {}\n");
    nob_sb_append_cstr(&sb, "#else\n");
    nob_sb_append_cstr(&sb, "void setUp(void) {}\nvoid tearDown(void) {}\n");
    nob_sb_append_cstr(&sb, "#endif\n\n");
    for (size_t i = 0; i < count; ++i) {
        nob_sb_appendf(&sb, "void %s(void);\n", items[i].name);
    }
    nob_sb_append_cstr(&sb, "\nint tests_");
    nob_sb_append_cstr(&sb, group);
    nob_sb_append_cstr(&sb, "_run(int argc, char **argv)\n{\n");
    nob_sb_append_cstr(&sb, "    const char *filter = NULL;\n");
    nob_sb_append_cstr(&sb, "    for (int i = 1; i + 1 < argc; ++i) {\n");
    nob_sb_append_cstr(&sb, "        if (strcmp(argv[i], \"--test-substr\") == 0) {\n");
    nob_sb_append_cstr(&sb, "            filter = argv[i + 1];\n");
    nob_sb_append_cstr(&sb, "        }\n");
    nob_sb_append_cstr(&sb, "    }\n\n");
    nob_sb_append_cstr(&sb, "    UNITY_BEGIN();\n\n");
    for (size_t i = 0; i < count; ++i) {
        nob_sb_appendf(&sb,
            "    if (!filter || strstr(\"%s\", filter)) RUN_TEST(%s);\n",
            items[i].name,
            items[i].name
        );
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
