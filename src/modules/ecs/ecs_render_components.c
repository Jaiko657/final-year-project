#include "modules/ecs/ecs_internal.h"
#include "modules/ecs/ecs_render.h"
#include "modules/asset/asset.h"

static void ecs_sprite_destroy_hook(int idx)
{
    if (idx < 0 || idx >= ECS_MAX_ENTITIES) return;
    if (!(ecs_mask[idx] & CMP_SPR)) return;
    if (asset_texture_valid(cmp_spr[idx].tex)) {
        asset_release_texture(cmp_spr[idx].tex);
        cmp_spr[idx].tex = (tex_handle_t){0, 0};
    }
}

void ecs_register_render_component_hooks(void)
{
    ecs_register_component_destroy_hook(ENUM_SPR, ecs_sprite_destroy_hook);
}

void cmp_add_sprite_handle(ecs_entity_t e, tex_handle_t h, rectf src, float ox, float oy)
{
    int i = ent_index_checked(e);
    if (i < 0) return;
    if (asset_texture_valid(h)) asset_addref_texture(h);
    cmp_spr[i] = (cmp_sprite_t){ h, src, ox, oy };
    ecs_mask[i] |= CMP_SPR;
}

void cmp_add_sprite_path(ecs_entity_t e, const char* path, rectf src, float ox, float oy)
{
    tex_handle_t h = asset_acquire_texture(path);
    cmp_add_sprite_handle(e, h, src, ox, oy);
    asset_release_texture(h);
}
