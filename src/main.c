#include "includes/engine.h"

int main(void)
{
    if (!engine_init("raylib + ECS: coins, vendor, hat")) {
        return 1;
    }

    int code = engine_run();
    engine_shutdown();
    return code;
}
