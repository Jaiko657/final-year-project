#include "plugin_api.h"

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} Str_List;

static bool cstr_ends_with(const char *s, const char *suffix)
{
    if (!s || !suffix) return false;
    size_t n = strlen(s);
    size_t m = strlen(suffix);
    if (m > n) return false;
    return memcmp(s + (n - m), suffix, m) == 0;
}

static void str_list_push(Str_List *list, const char *s)
{
    if (list->count >= list->cap) {
        size_t new_cap = (list->cap == 0) ? 16 : list->cap * 2;
        char **new_items = (char **)realloc(list->items, new_cap * sizeof(*new_items));
        if (!new_items) return;
        list->items = new_items;
        list->cap = new_cap;
    }
    list->items[list->count++] = strdup(s);
}

static int cmp_cstrs(const void *a, const void *b)
{
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

static void str_list_free(Str_List *list)
{
    for (size_t i = 0; i < list->count; ++i) free(list->items[i]);
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [--list] [--plugin-substr s] [--test-substr s] [--fail-fast] [--verbose] [--plugins-dir path]\n",
        argv0
    );
}

int main(int argc, char **argv)
{
    const char *plugins_dir = "build/tests/plugins";
    const char *plugin_substr = NULL;
    bool list_only = false;
    bool fail_fast = false;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--list") == 0) list_only = true;
        else if (strcmp(argv[i], "--fail-fast") == 0) fail_fast = true;
        else if (strcmp(argv[i], "--verbose") == 0) verbose = true;
        else if (strcmp(argv[i], "--plugin-substr") == 0 && i + 1 < argc) {
            plugin_substr = argv[++i];
        } else if (strcmp(argv[i], "--plugins-dir") == 0 && i + 1 < argc) {
            plugins_dir = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

    DIR *dir = opendir(plugins_dir);
    if (!dir) {
        fprintf(stderr, "Failed to open plugins dir '%s': %s\n", plugins_dir, strerror(errno));
        return 1;
    }

    Str_List plugins = {0};
    struct dirent *ent = NULL;
    while ((ent = readdir(dir)) != NULL) {
        if (!cstr_ends_with(ent->d_name, ".so")) continue;
        if (plugin_substr && !strstr(ent->d_name, plugin_substr)) continue;
        str_list_push(&plugins, ent->d_name);
    }
    closedir(dir);

    if (plugins.count == 0) {
        fprintf(stderr, "No plugins found in '%s'\n", plugins_dir);
        str_list_free(&plugins);
        return 1;
    }

    qsort(plugins.items, plugins.count, sizeof(*plugins.items), cmp_cstrs);

    if (list_only) {
        for (size_t i = 0; i < plugins.count; ++i) {
            char *name = plugins.items[i];
            size_t len = strlen(name);
            if (len > 3) {
                name[len - 3] = '\0';
                printf("%s\n", name);
                name[len - 3] = '.';
            } else {
                printf("%s\n", name);
            }
        }
        str_list_free(&plugins);
        return 0;
    }

    int failures = 0;
    for (size_t i = 0; i < plugins.count; ++i) {
        const char *fname = plugins.items[i];
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", plugins_dir, fname);

        if (verbose) printf("[test_all] loading %s\n", path);
        void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            fprintf(stderr, "dlopen failed for %s: %s\n", path, dlerror());
            failures++;
            if (fail_fast) break;
            continue;
        }

        char sym[256];
        size_t len = strlen(fname);
        if (len < 4 || len >= sizeof(sym) - 5) {
            fprintf(stderr, "Invalid plugin name: %s\n", fname);
            dlclose(handle);
            failures++;
            if (fail_fast) break;
            continue;
        }
        memcpy(sym, fname, len - 3);
        sym[len - 3] = '\0';
        strncat(sym, "_run", sizeof(sym) - strlen(sym) - 1);

        dlerror();
        tests_run_fn run_fn = (tests_run_fn)dlsym(handle, sym);
        const char *err = dlerror();
        if (err) {
            fprintf(stderr, "dlsym failed for %s in %s: %s\n", sym, path, err);
            dlclose(handle);
            failures++;
            if (fail_fast) break;
            continue;
        }

        if (verbose) printf("[test_all] running %s\n", sym);
        int rc = run_fn(argc, argv);
        if (rc != 0) {
            fprintf(stderr, "[test_all] %s failed with code %d\n", sym, rc);
            failures++;
            if (fail_fast) {
                dlclose(handle);
                break;
            }
        }
        dlclose(handle);
    }

    str_list_free(&plugins);
    return failures == 0 ? 0 : 1;
}
