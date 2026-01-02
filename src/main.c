#include "modules/core/engine.h"

int main(void)
{
    if (!engine_init("raylib + ECS: TARDAS MVP")) {
        return 1;
    }

    int code = engine_run();
    engine_shutdown();
    return code;
}
