#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct { float x, y; } v2f;
typedef struct { float x, y, w, h; } rectf;
typedef struct { float r, g, b, a; } colorf;

// Optional helpers
static inline v2f v2f_make(float x, float y){ return (v2f){x,y}; }
static inline rectf rectf_xywh(float x,float y,float w,float h){ return (rectf){x,y,w,h}; }
