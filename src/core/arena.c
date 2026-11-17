#include "core/arena.h"

#include "core/log.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool arena_init(Arena *a, usize capacity) {
    a->base = (u8 *)malloc(capacity);
    a->capacity = a->base ? capacity : 0;
    a->used = 0;
    return a->base != NULL;
}

void arena_free(Arena *a) {
    free(a->base);
    a->base = NULL;
    a->capacity = 0;
    a->used = 0;
}

void arena_reset(Arena *a) {
    a->used = 0;
}

void *arena_push(Arena *a, usize size, usize align) {
    ASSERT(align != 0 && (align & (align - 1)) == 0);
    uintptr_t cur = (uintptr_t)a->base + a->used;
    uintptr_t aligned = (cur + (align - 1)) & ~(uintptr_t)(align - 1);
    usize offset = (usize)(aligned - (uintptr_t)a->base);
    if (offset > a->capacity || size > a->capacity - offset) {
        return NULL;
    }
    a->used = offset + size;
    memset(a->base + offset, 0, size);
    return a->base + offset;
}
