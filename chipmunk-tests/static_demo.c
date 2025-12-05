#include <stdio.h>
#include <chipmunk/chipmunk.h>

int main(void)
{
    cpVect gravity = cpv(0.0f, -100.0f);

    cpSpace* space = cpSpaceNew();
    cpSpaceSetSleepTimeThreshold(space, INFINITY);
    if (!space) return 1;
    cpSpaceSetGravity(space, gravity);

    cpShape* ground = cpSegmentShapeNew(
        cpSpaceGetStaticBody(space),
        cpv(-20.0f, 5.0f),
        cpv(20.0f, -5.0f),
        0.0f
    );
    cpShapeSetFriction(ground, 1.0f);
    cpSpaceAddShape(space, ground);

    const cpFloat radius = 5.0f;
    const cpFloat mass = 1.0f;
    const cpFloat moment = cpMomentForCircle(mass, 0.0f, radius, cpvzero);

    cpBody* ball_body = cpSpaceAddBody(space, cpBodyNew(mass, moment));
    cpBodySetPosition(ball_body, cpv(0.0f, 15.0f));

    cpShape* ball_shape = cpSpaceAddShape(space, cpCircleShapeNew(ball_body, radius, cpvzero));
    cpShapeSetFriction(ball_shape, 0.7f);

    const cpFloat time_step = 1.0f / 60.0f;
    for (cpFloat time = 0.0f; time < 2.0f; time += time_step) {
        cpVect pos = cpBodyGetPosition(ball_body);
        cpVect vel = cpBodyGetVelocity(ball_body);
        printf(
            "Time %5.2f | pos (%5.2f, %5.2f) | vel (%5.2f, %5.2f)\n",
            time, pos.x, pos.y, vel.x, vel.y
        );

        cpSpaceStep(space, time_step);
    }

    cpShapeFree(ball_shape);
    cpBodyFree(ball_body);
    cpShapeFree(ground);
    cpSpaceFree(space);

    return 0;
}
