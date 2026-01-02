#include "modules/renderer/renderer_internal.h"
#include "modules/systems/systems_registration.h"

static void renderer_noop(void) { }

SYSTEMS_ADAPT_VOID(sys_render_begin_adapt, renderer_noop)
SYSTEMS_ADAPT_VOID(sys_render_world_prepare_adapt, renderer_noop)
SYSTEMS_ADAPT_VOID(sys_render_world_base_adapt, renderer_noop)
SYSTEMS_ADAPT_VOID(sys_render_world_fx_adapt, renderer_noop)
SYSTEMS_ADAPT_VOID(sys_render_world_sprites_adapt, renderer_noop)
SYSTEMS_ADAPT_VOID(sys_render_world_overlays_adapt, renderer_noop)
SYSTEMS_ADAPT_VOID(sys_render_world_end_adapt, renderer_noop)
SYSTEMS_ADAPT_VOID(sys_render_ui_adapt, renderer_noop)
SYSTEMS_ADAPT_VOID(sys_render_end_adapt, renderer_noop)

void renderer_frame_begin(void) { }
void renderer_frame_end(void) { }
void renderer_world_prepare(const render_view_t* view) { (void)view; }
void renderer_world_base(const render_view_t* view) { (void)view; }
void renderer_world_fx(const render_view_t* view) { (void)view; }
void renderer_world_sprites(const render_view_t* view) { (void)view; }
void renderer_world_overlays(const render_view_t* view) { (void)view; }
void renderer_world_end(void) { }
void renderer_ui(const render_view_t* view) { (void)view; }
