#include "includes/engine.h"

int main(void)
{
    const int W = 800;
    const int H = 450;

    if (!engine_init(W, H, "raylib + ECS: coins, vendor, hat")) {
        return 1;
    }

    int code = engine_run();
    engine_shutdown();
    return code;
}
