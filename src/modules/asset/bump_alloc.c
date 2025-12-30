#include "modules/asset/bump_alloc.h"

#include <stdlib.h>
#include <string.h>

bool bump_init(bump_alloc_t *b, size_t capacity)
{
    if (!b) return false;
    bump_free(b);
    b->data = (uint8_t *)malloc(capacity);
    if (!b->data) {
        b->capacity = 0;
        b->offset = 0;
        return false;
    }
    b->capacity = capacity;
    b->offset = 0;
    return true;
}

void bump_reset(bump_alloc_t *b)
{
    if (!b) return;
    b->offset = 0;
}

void bump_free(bump_alloc_t *b)
{
    if (!b) return;
    free(b->data);
    b->data = NULL;
    b->capacity = 0;
    b->offset = 0;
}

static size_t align_up(size_t x, size_t align)
{
    size_t mask = align - 1;
    return (x + mask) & ~mask;
}

void *bump_alloc_aligned(bump_alloc_t *b, size_t size, size_t align)
{
    if (!b || !b->data || align == 0) return NULL;
    size_t start = align_up(b->offset, align);
    size_t end = start + size;
    if (end > b->capacity) return NULL;
    void *ptr = b->data + start;
    b->offset = end;
    return ptr;
}
