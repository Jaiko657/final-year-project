#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#define NOB_IMPLEMENTATION
#include "../third_party/nob.h"

#include <string.h>

static bool compile_exe(const char *cc, const char *cflags, const char *includes, const char *src, const char *out, const char *ldflags)
{
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "sh", "-lc",
        nob_temp_sprintf("%s %s %s %s -o %s %s",
            cc,
            cflags ? cflags : "",
            includes ? includes : "",
            src,
            out,
            ldflags ? ldflags : ""
        )
    );
    return nob_cmd_run_sync_and_reset(&cmd);
}

static bool build_tool(const char *cc, const char *src, const char *out)
{
    const char *cflags = "-std=c99 -Wall -Wextra -O0 -g -D_POSIX_C_SOURCE=200809L";
    return compile_exe(cc, cflags, NULL, src, out, NULL);
}

static bool run_tool(const char *tool_path, const char *arg)
{
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, tool_path);
    if (arg && arg[0] != '\0') {
        nob_cmd_append(&cmd, arg);
    }
    return nob_cmd_run_sync_and_reset(&cmd);
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    bool run_tests = false;
    bool coverage = false;
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--run") == 0) run_tests = true;
        else if (strcmp(argv[i], "--coverage") == 0) coverage = true;
        else if (strcmp(argv[i], "--verbose") == 0) verbose = true;
    }
    if (coverage) run_tests = true;

    nob_minimal_log_level = verbose ? NOB_INFO : NOB_WARNING;

    if (!nob_mkdir_if_not_exists("build/tests")) return 1;
    if (!nob_mkdir_if_not_exists("build/tests/bin")) return 1;
    if (!nob_mkdir_if_not_exists("build/tests/gen")) return 1;
    if (!nob_mkdir_if_not_exists("build/tests/obj")) return 1;
    if (!nob_mkdir_if_not_exists("build/tests/plugins")) return 1;
    if (coverage) {
        if (!nob_mkdir_if_not_exists("build/tests/coverage")) return 1;
        Nob_Cmd clean = {0};
        nob_cmd_append(&clean, "sh", "-lc",
            "rm -f build/tests/obj/*/*.gcda build/tests/obj/*/*.gcov");
        if (!nob_cmd_run_sync_and_reset(&clean)) return 1;
    }

    {
        Nob_Cmd clean = {0};
        nob_cmd_append(&clean, "sh", "-lc", "rm -f build/tests/plugins/*.so build/tests/gen/tests_*_runner.c");
        if (!nob_cmd_run_sync_and_reset(&clean)) return 1;
    }

    const char *cc = getenv("CC");
    if (!cc || cc[0] == '\0') cc = "cc";

    if (!compile_exe(
            cc,
            coverage
                ? "-std=c99 -Wall -Wextra -O0 -g --coverage -D_POSIX_C_SOURCE=200809L"
                : "-std=c99 -Wall -Wextra -O0 -g -D_POSIX_C_SOURCE=200809L",
            "-I tests/unit/test_runner",
            "tests/unit/test_runner/test_all.c",
            "build/tests/unit_tests",
            "-ldl"
        )) {
        return 1;
    }

    if (!build_tool(cc, "tests/unit/core/camera/build_camera.c", "build/tests/bin/build_camera")) return 1;
    if (!run_tool("build/tests/bin/build_camera", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/core/cmp_print/build_cmp_print.c", "build/tests/bin/build_cmp_print")) return 1;
    if (!run_tool("build/tests/bin/build_cmp_print", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/asset/bump_alloc/build_bump_alloc.c", "build/tests/bin/build_bump_alloc")) return 1;
    if (!run_tool("build/tests/bin/build_bump_alloc", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/asset/build_asset.c", "build/tests/bin/build_asset")) return 1;
    if (!run_tool("build/tests/bin/build_asset", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/core/toast/build_toast.c", "build/tests/bin/build_toast")) return 1;
    if (!run_tool("build/tests/bin/build_toast", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/core/logger/build_logger.c", "build/tests/bin/build_logger")) return 1;
    if (!run_tool("build/tests/bin/build_logger", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/core/dynarray/build_dynarray.c", "build/tests/bin/build_dynarray")) return 1;
    if (!run_tool("build/tests/bin/build_dynarray", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/core/input/build_input.c", "build/tests/bin/build_input")) return 1;
    if (!run_tool("build/tests/bin/build_input", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/core/debug_hotkeys/build_debug_hotkeys.c", "build/tests/bin/build_debug_hotkeys")) return 1;
    if (!run_tool("build/tests/bin/build_debug_hotkeys", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/core/engine/build_engine.c", "build/tests/bin/build_engine")) return 1;
    if (!run_tool("build/tests/bin/build_engine", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/core/time/build_time.c", "build/tests/bin/build_time")) return 1;
    if (!run_tool("build/tests/bin/build_time", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/core/logger_raylib_adapter/build_logger_raylib_adapter.c", "build/tests/bin/build_logger_raylib_adapter")) return 1;
    if (!run_tool("build/tests/bin/build_logger_raylib_adapter", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/ecs/build_ecs.c", "build/tests/bin/build_ecs")) return 1;
    if (!run_tool("build/tests/bin/build_ecs", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/ecs/core/build_ecs_core.c", "build/tests/bin/build_ecs_core")) return 1;
    if (!run_tool("build/tests/bin/build_ecs_core", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/ecs/iterators/build_ecs_iterators.c", "build/tests/bin/build_ecs_iterators")) return 1;
    if (!run_tool("build/tests/bin/build_ecs_iterators", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/ecs/anim/build_ecs_anim.c", "build/tests/bin/build_ecs_anim")) return 1;
    if (!run_tool("build/tests/bin/build_ecs_anim", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/ecs/physics/build_ecs_physics.c", "build/tests/bin/build_ecs_physics")) return 1;
    if (!run_tool("build/tests/bin/build_ecs_physics", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/ecs/system_domains/build_ecs_system_domains.c", "build/tests/bin/build_ecs_system_domains")) return 1;
    if (!run_tool("build/tests/bin/build_ecs_system_domains", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/ecs/registration/build_ecs_registration.c", "build/tests/bin/build_ecs_registration")) return 1;
    if (!run_tool("build/tests/bin/build_ecs_registration", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/ecs/proximity/build_ecs_proximity.c", "build/tests/bin/build_ecs_proximity")) return 1;
    if (!run_tool("build/tests/bin/build_ecs_proximity", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/ecs/liftable/build_ecs_liftable.c", "build/tests/bin/build_ecs_liftable")) return 1;
    if (!run_tool("build/tests/bin/build_ecs_liftable", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/ecs/game/build_ecs_game.c", "build/tests/bin/build_ecs_game")) return 1;
    if (!run_tool("build/tests/bin/build_ecs_game", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/ecs/prefab_loading/build_ecs_prefab_loading.c", "build/tests/bin/build_ecs_prefab_loading")) return 1;
    if (!run_tool("build/tests/bin/build_ecs_prefab_loading", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/prefab/build_prefab.c", "build/tests/bin/build_prefab")) return 1;
    if (!run_tool("build/tests/bin/build_prefab", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/tiled/build_tiled.c", "build/tests/bin/build_tiled")) return 1;
    if (!run_tool("build/tests/bin/build_tiled", coverage ? "--coverage" : NULL)) return 1;

    if (!build_tool(cc, "tests/unit/world/build_world.c", "build/tests/bin/build_world")) return 1;
    if (!run_tool("build/tests/bin/build_world", coverage ? "--coverage" : NULL)) return 1;

    if (run_tests) {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "build/tests/unit_tests");
        if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    }

    if (coverage) {
        const char *cover_cmd =
            "set -euo pipefail; "
            "exec 2> >(grep -v 'Duplicate specification' >&2); "
            "if command -v lcov >/dev/null 2>&1 && command -v genhtml >/dev/null 2>&1; then "
            "  lcov --capture --directory build/tests/obj --output-file build/tests/coverage/lcov.info >/dev/null; "
            "  lcov --remove build/tests/coverage/lcov.info '*/third_party/*' '*/tests/*' --output-file build/tests/coverage/lcov.info.cleaned >/dev/null; "
            "  genhtml build/tests/coverage/lcov.info.cleaned --output-directory build/tests/coverage/html >/dev/null; "
            "  echo \"[coverage] HTML: build/tests/coverage/html/index.html\"; "
            "else "
            "  echo \"[coverage] Install lcov+genhtml to generate HTML (coverage data is in build/tests/obj).\"; "
            "fi";
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "bash", "-lc", cover_cmd);
        if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    }

    return 0;
}
