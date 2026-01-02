#include "modules/core/build_config.h"

#if DEBUG_BUILD
#include "modules/renderer/renderer.h"
#include "modules/renderer/renderer_internal.h"

bool renderer_toggle_ecs_colliders(void) { return false; }
bool renderer_toggle_phys_colliders(void) { return false; }
bool renderer_toggle_static_colliders(void) { return false; }
bool renderer_toggle_triggers(void) { return false; }
bool renderer_toggle_fps_overlay(void) { return false; }

void draw_debug_collision_overlays(const render_view_t* view) { (void)view; }
void draw_debug_trigger_overlays(const render_view_t* view) { (void)view; }
void renderer_debug_draw_ui(const render_view_t* view) { (void)view; }
#endif
