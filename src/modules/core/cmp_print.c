#include "modules/core/cmp_print.h"

#include "modules/core/logger.h"

static const char* facing_short(facing_t d)
{
    switch (d) {
        case DIR_NORTH:     return "N";
        case DIR_NORTHEAST: return "NE";
        case DIR_EAST:      return "E";
        case DIR_SOUTHEAST: return "SE";
        case DIR_SOUTH:     return "S";
        case DIR_SOUTHWEST: return "SW";
        case DIR_WEST:      return "W";
        case DIR_NORTHWEST: return "NW";
        default:            return "?";
    }
}

static const char* phys_type_short(PhysicsType t)
{
    switch (t) {
        case PHYS_NONE:      return "NONE";
        case PHYS_DYNAMIC:   return "DYN";
        case PHYS_KINEMATIC: return "KIN";
        case PHYS_STATIC:    return "STA";
        default:             return "?";
    }
}

static const char* billboard_state_short(billboard_state_t s)
{
    switch (s) {
        case BILLBOARD_INACTIVE: return "OFF";
        case BILLBOARD_ACTIVE:   return "ON";
        default:                 return "?";
    }
}

static const char* grav_gun_state_short(grav_gun_state_t s)
{
    switch (s) {
        case GRAV_GUN_STATE_FREE: return "FREE";
        case GRAV_GUN_STATE_HELD: return "HELD";
        default:                  return "?";
    }
}

static const char* door_state_short(int s)
{
    switch (s) {
        case DOOR_CLOSED:  return "CLOSED";
        case DOOR_OPENING: return "OPENING";
        case DOOR_OPEN:    return "OPEN";
        case DOOR_CLOSING: return "CLOSING";
        default:           return "?";
    }
}

static const char* indent_or_empty(const char* indent)
{
    return indent ? indent : "";
}

void cmp_print_position(const char* indent, const cmp_position_t* p)
{
    const char* prefix = indent_or_empty(indent);
    if (!p) {
        LOGC(LOGCAT_ECS, LOG_LVL_INFO, "%sPOS(null)", prefix);
        return;
    }
    LOGC(LOGCAT_ECS, LOG_LVL_INFO, "%sPOS(x=%.3f, y=%.3f)", prefix, p->x, p->y);
}

void cmp_print_velocity(const char* indent, const cmp_velocity_t* v)
{
    const char* prefix = indent_or_empty(indent);
    if (!v) {
        LOGC(LOGCAT_ECS, LOG_LVL_INFO, "%sVEL(null)", prefix);
        return;
    }
    const smoothed_facing_t* f = &v->facing;
    LOGC(
        LOGCAT_ECS,
        LOG_LVL_INFO,
        "%sVEL(x=%.3f, y=%.3f, raw=%s, facing=%s, cand=%s, cand_t=%.3f)",
        prefix,
        v->x,
        v->y,
        facing_short(f->rawDir),
        facing_short(f->facingDir),
        facing_short(f->candidateDir),
        f->candidateTime
    );
}

void cmp_print_phys_body(const char* indent, const cmp_phys_body_t* b)
{
    const char* prefix = indent_or_empty(indent);
    if (!b) {
        LOGC(LOGCAT_ECS, LOG_LVL_INFO, "%sPHYS_BODY(null)", prefix);
        return;
    }
    LOGC(
        LOGCAT_ECS,
        LOG_LVL_INFO,
        "%sPHYS_BODY(type=%s, mass=%.3f, inv=%.3f, cat=0x%X, mask=0x%X, created=%d)",
        prefix,
        phys_type_short(b->type),
        b->mass,
        b->inv_mass,
        b->category_bits,
        b->mask_bits,
        b->created ? 1 : 0
    );
}

void cmp_print_sprite(const char* indent, const cmp_sprite_t* s)
{
    const char* prefix = indent_or_empty(indent);
    if (!s) {
        LOGC(LOGCAT_ECS, LOG_LVL_INFO, "%sSPR(null)", prefix);
        return;
    }
    LOGC(
        LOGCAT_ECS,
        LOG_LVL_INFO,
        "%sSPR(tex=(%u,%u), src=[%.1f,%.1f,%.1f,%.1f], origin=(%.1f,%.1f))",
        prefix,
        s->tex.idx,
        s->tex.gen,
        s->src.x,
        s->src.y,
        s->src.w,
        s->src.h,
        s->ox,
        s->oy
    );
}

void cmp_print_anim(const char* indent, const cmp_anim_t* a)
{
    const char* prefix = indent_or_empty(indent);
    if (!a) {
        LOGC(LOGCAT_ECS, LOG_LVL_INFO, "%sANIM(null)", prefix);
        return;
    }
    LOGC(
        LOGCAT_ECS,
        LOG_LVL_INFO,
        "%sANIM(frame=%dx%d, anim=%d/%d, frame_i=%d, frame_dt=%.3f, t=%.3f)",
        prefix,
        a->frame_w,
        a->frame_h,
        a->current_anim,
        a->anim_count,
        a->frame_index,
        a->frame_duration,
        a->current_time
    );
}

void cmp_print_player(const char* indent)
{
    const char* prefix = indent_or_empty(indent);
    LOGC(LOGCAT_ECS, LOG_LVL_INFO, "%sPLAYER()", prefix);
}

void cmp_print_storage(const char* indent, int plastic, int capacity)
{
    const char* prefix = indent_or_empty(indent);
    LOGC(LOGCAT_ECS, LOG_LVL_INFO, "%sSTORAGE(plastic=%d, capacity=%d)", prefix, plastic, capacity);
}

void cmp_print_follow(const char* indent, const cmp_follow_t* f)
{
    const char* prefix = indent_or_empty(indent);
    if (!f) {
        LOGC(LOGCAT_ECS, LOG_LVL_INFO, "%sFOLLOW(null)", prefix);
        return;
    }
    if (f->has_last_seen) {
        LOGC(
            LOGCAT_ECS,
            LOG_LVL_INFO,
            "%sFOLLOW(target=%u, dist=%.1f, max=%.1f, vision=%.1f, last=(%.1f,%.1f))",
            prefix,
            f->target.idx,
            f->desired_distance,
            f->max_speed,
            f->vision_range,
            f->last_seen_x,
            f->last_seen_y
        );
        return;
    }
    LOGC(
        LOGCAT_ECS,
        LOG_LVL_INFO,
        "%sFOLLOW(target=%u, dist=%.1f, max=%.1f, vision=%.1f)",
        prefix,
        f->target.idx,
        f->desired_distance,
        f->max_speed,
        f->vision_range
    );
}

void cmp_print_collider(const char* indent, const cmp_collider_t* c)
{
    const char* prefix = indent_or_empty(indent);
    if (!c) {
        LOGC(LOGCAT_ECS, LOG_LVL_INFO, "%sCOL(null)", prefix);
        return;
    }
    LOGC(LOGCAT_ECS, LOG_LVL_INFO, "%sCOL(hx=%.2f, hy=%.2f)", prefix, c->hx, c->hy);
}

void cmp_print_grav_gun(const char* indent, const cmp_grav_gun_t* g)
{
    const char* prefix = indent_or_empty(indent);
    if (!g) {
        LOGC(LOGCAT_ECS, LOG_LVL_INFO, "%sGRAV_GUN(null)", prefix);
        return;
    }
    LOGC(
        LOGCAT_ECS,
        LOG_LVL_INFO,
        "%sGRAV_GUN(state=%s, holder=%u, drop=%d, follow=%.2f, max=%.1f, break=%.1f, hold_v=(%.1f,%.1f))",
        prefix,
        grav_gun_state_short(g->state),
        g->holder.idx,
        g->just_dropped ? 1 : 0,
        g->follow_gain,
        g->max_speed,
        g->breakoff_distance,
        g->hold_vel_x,
        g->hold_vel_y
    );
}

void cmp_print_trigger(const char* indent, const cmp_trigger_t* t)
{
    const char* prefix = indent_or_empty(indent);
    if (!t) {
        LOGC(LOGCAT_ECS, LOG_LVL_INFO, "%sTRIGGER(null)", prefix);
        return;
    }
    LOGC(
        LOGCAT_ECS,
        LOG_LVL_INFO,
        "%sTRIGGER(pad=%.2f, target_mask=0x%X)",
        prefix,
        t->pad,
        t->target_mask
    );
}

void cmp_print_billboard(const char* indent, const cmp_billboard_t* b)
{
    const char* prefix = indent_or_empty(indent);
    if (!b) {
        LOGC(LOGCAT_ECS, LOG_LVL_INFO, "%sBILLBOARD(null)", prefix);
        return;
    }
    LOGC(
        LOGCAT_ECS,
        LOG_LVL_INFO,
        "%sBILLBOARD(text=\"%.24s\", y_off=%.1f, linger=%.1f, t=%.1f, state=%s)",
        prefix,
        b->text,
        b->y_offset,
        b->linger,
        b->timer,
        billboard_state_short(b->state)
    );
}

void cmp_print_door(const char* indent, const cmp_door_t* d)
{
    const char* prefix = indent_or_empty(indent);
    if (!d) {
        LOGC(LOGCAT_ECS, LOG_LVL_INFO, "%sDOOR(null)", prefix);
        return;
    }
    LOGC(
        LOGCAT_ECS,
        LOG_LVL_INFO,
        "%sDOOR(prox=%.1f state=%s anim_t=%.1f intent_open=%d handle=0x%X)",
        prefix,
        d->prox_radius,
        door_state_short(d->state),
        d->anim_time_ms,
        d->intent_open ? 1 : 0,
        d->world_handle
    );
}
