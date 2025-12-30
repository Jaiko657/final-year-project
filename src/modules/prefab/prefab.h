#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "modules/ecs/ecs.h"

typedef struct {
    char* name;
    char* value;
} prefab_kv_t;

typedef struct {
    char* name;
    anim_frame_coord_t* frames;
    size_t frame_count;
} prefab_anim_seq_t;

typedef struct {
    int frame_w;
    int frame_h;
    float fps;
    prefab_anim_seq_t* seqs;
    size_t seq_count;
} prefab_anim_def_t;

typedef struct prefab_component_t {
    ComponentEnum id;
    char* type_name;
    prefab_kv_t* props;
    size_t prop_count;
    prefab_anim_def_t* anim;
    bool override_after_spawn;
} prefab_component_t;

typedef struct prefab_t {
    char* name;
    prefab_component_t* components;
    size_t component_count;
} prefab_t;

bool prefab_load(const char* path, prefab_t* out_prefab);
void prefab_free(prefab_t* prefab);
