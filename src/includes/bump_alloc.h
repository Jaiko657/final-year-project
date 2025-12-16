#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t *data;
    size_t   capacity;
    size_t   offset;
} bump_alloc_t;

bool   bump_init(bump_alloc_t *b, size_t capacity);
void   bump_reset(bump_alloc_t *b);
void   bump_free(bump_alloc_t *b);
void  *bump_alloc_aligned(bump_alloc_t *b, size_t size, size_t align);

// Convenience for typed allocations
#define bump_alloc_type(b, type, count) \
    ((type *)bump_alloc_aligned((b), (count) * sizeof(type), __alignof__(type)))
