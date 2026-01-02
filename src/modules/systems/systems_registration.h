#pragma once
#include "modules/core/input.h"

#define SYSTEMS_ADAPT_DT(name, fn) \
    void name(float dt, const input_t* in) { (void)in; fn(dt); }
#define SYSTEMS_ADAPT_VOID(name, fn) \
    void name(float dt, const input_t* in) { (void)dt; (void)in; fn(); }
#define SYSTEMS_ADAPT_INPUT(name, fn) \
    void name(float dt, const input_t* in) { (void)dt; fn(in); }
#define SYSTEMS_ADAPT_BOTH(name, fn) \
    void name(float dt, const input_t* in) { fn(dt, in); }

void systems_registration_init(void);
