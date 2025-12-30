#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Simple dynamic array macros (type-safe via macros)
#ifndef DA_INIT_CAP
#define DA_INIT_CAP 8
#endif

#define DA(type) \
    struct {     \
        type  *data;     \
        size_t size;     \
        size_t capacity; \
    }

#define DA_CLEAR(da) do { (da)->size = 0; } while (0)

#define DA_FREE(da) do {            \
    free((da)->data);               \
    (da)->data = NULL;              \
    (da)->size = 0;                 \
    (da)->capacity = 0;             \
} while (0)

#define DA_GROW(da) do {                                                    \
    size_t _new_cap = (da)->capacity ? (da)->capacity * 2 : DA_INIT_CAP;    \
    void* _tmp = realloc((da)->data, _new_cap * sizeof(*(da)->data));       \
    assert(_tmp && "DA_GROW realloc failed");                               \
    (da)->data = _tmp;                                                      \
    (da)->capacity = _new_cap;                                              \
} while (0)

#define DA_RESERVE(da, n) do {                                              \
    size_t _need = (size_t)(n);                                             \
    if ((da)->capacity < _need) {                                           \
        size_t _cap = (da)->capacity ? (da)->capacity : DA_INIT_CAP;        \
        while (_cap < _need) _cap *= 2;                                     \
        void* _tmp = realloc((da)->data, _cap * sizeof(*(da)->data));       \
        assert(_tmp && "DA_RESERVE realloc failed");                        \
        (da)->data = _tmp;                                                  \
        (da)->capacity = _cap;                                              \
    }                                                                       \
} while (0)

#define DA_APPEND(da, value) do {                                           \
    if ((da)->size >= (da)->capacity) {                                     \
        DA_GROW(da);                                                        \
    }                                                                       \
    (da)->data[(da)->size++] = (value);                                     \
} while (0)

