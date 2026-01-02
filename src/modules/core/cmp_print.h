#pragma once

#include <stdbool.h>

#include "modules/ecs/ecs_internal.h"

void cmp_print_position(const char* indent, const cmp_position_t* p);
void cmp_print_velocity(const char* indent, const cmp_velocity_t* v);
void cmp_print_phys_body(const char* indent, const cmp_phys_body_t* b);
void cmp_print_sprite(const char* indent, const cmp_sprite_t* s);
void cmp_print_anim(const char* indent, const cmp_anim_t* a);
void cmp_print_player(const char* indent);
void cmp_print_item(const char* indent, int kind);
void cmp_print_inventory(const char* indent, int coins, bool has_hat);
void cmp_print_vendor(const char* indent, int sells, int price);
void cmp_print_follow(const char* indent, const cmp_follow_t* f);
void cmp_print_collider(const char* indent, const cmp_collider_t* c);
void cmp_print_grav_gun(const char* indent, const cmp_grav_gun_t* g);
void cmp_print_trigger(const char* indent, const cmp_trigger_t* t);
void cmp_print_billboard(const char* indent, const cmp_billboard_t* b);
void cmp_print_door(const char* indent, const cmp_door_t* d);
