// Linear (bump) allocator. Allocated once, reset or freed in bulk; never grows.
#pragma once

#include "core/types.h"

typedef struct Arena {
    u8 *base;
    usize capacity;
    usize used;
} Arena;

bool arena_init(Arena *a, usize capacity);
void arena_free(Arena *a);
void arena_reset(Arena *a);

// Returns zeroed memory, or NULL when the arena is full. `align` must be a power of two.
void *arena_push(Arena *a, usize size, usize align);

#define ARENA_PUSH_STRUCT(a, T) ((T *)arena_push((a), sizeof(T), _Alignof(T)))
#define ARENA_PUSH_ARRAY(a, T, n) ((T *)arena_push((a), sizeof(T) * (n), _Alignof(T)))
