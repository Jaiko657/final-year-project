#pragma once

// Central toggle for debug-only codepaths.
#ifndef DEBUG_BUILD
#define DEBUG_BUILD 0
#endif

#ifndef DEBUG_COLLISION
#define DEBUG_COLLISION DEBUG_BUILD
#endif

#ifndef DEBUG_TRIGGERS
#define DEBUG_TRIGGERS DEBUG_BUILD
#endif

#ifndef DEBUG_FPS
#define DEBUG_FPS DEBUG_BUILD
#endif
