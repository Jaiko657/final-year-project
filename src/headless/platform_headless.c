#include "modules/core/platform.h"
#include "modules/core/logger.h"

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static bool dir_exists(const char* path)
{
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void log_working_dir(void)
{
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf))) {
        LOGC(LOGCAT_MAIN, LOG_LVL_INFO, "SYSTEM: Working Directory: %s", buf);
    }
}

void platform_init(void)
{
    // Normalize CWD so relative paths like "assets/..." work when running build/src/game_headless.
    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n > 0) {
        exe[n] = '\0';
        char* last_slash = strrchr(exe, '/');
        if (last_slash) {
            *last_slash = '\0';
            (void)chdir(exe);
        }
    }

    for (int i = 0; i < 4; ++i) {
        if (dir_exists("assets")) break;
        if (chdir("..") != 0) break;
    }

    log_working_dir();
}

bool platform_should_close(void)
{
    static int frames = 0;
    static int max_frames = -1;

    if (max_frames < 0) {
        const char* env = getenv("HEADLESS_MAX_TICKS");
        max_frames = env ? atoi(env) : 600;
        if (max_frames <= 0) max_frames = 1;
    }

    return (frames++ >= max_frames);
}
