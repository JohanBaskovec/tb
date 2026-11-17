#include "game/entity.h"

#include "core/log.h"

#include <string.h>

void pool_init(EntityPool *p) {
    memset(p, 0, sizeof(*p));
    // Popped from the end: the first creation gets slot 1, then 2, ...
    for (u16 i = 0; i < MAX_ENTITIES; i++) {
        p->free_list[i] = (u16)(MAX_ENTITIES - i);
    }
    p->free_count = MAX_ENTITIES;
}

EntityId pool_create(EntityPool *p, EntityType type) {
    ASSERT(type != ENT_NONE && type < ENT_TYPE_COUNT);
    if (p->free_count == 0) {
        return ENTITY_NONE;
    }
    const u16 index = p->free_list[--p->free_count];
    Entity *e = &p->slots[index];
    u16 generation = (u16)(e->generation + 1);
    if (generation == 0) { // wrapped: 0 is reserved for "none"
        generation = 1;
    }
    memset(e, 0, sizeof(*e));
    e->type = type;
    e->generation = generation;
    p->live_count++;
    return (EntityId){index, generation};
}

Entity *pool_get(EntityPool *p, EntityId id) {
    if (id.index == 0 || id.index > MAX_ENTITIES) {
        return NULL;
    }
    Entity *e = &p->slots[id.index];
    if (e->type == ENT_NONE || e->generation != id.generation) {
        return NULL;
    }
    return e;
}

const Entity *pool_get_const(const EntityPool *p, EntityId id) {
    if (id.index == 0 || id.index > MAX_ENTITIES) {
        return NULL;
    }
    const Entity *e = &p->slots[id.index];
    if (e->type == ENT_NONE || e->generation != id.generation) {
        return NULL;
    }
    return e;
}

void pool_mark_destroy(EntityPool *p, EntityId id) {
    Entity *e = pool_get(p, id);
    if (e) {
        e->pending_destroy = true;
    }
}

void pool_flush(EntityPool *p) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        Entity *e = &p->slots[i];
        if (e->type != ENT_NONE && e->pending_destroy) {
            e->type = ENT_NONE; // generation is kept: old ids stay stale
            e->pending_destroy = false;
            p->free_list[p->free_count++] = i;
            p->live_count--;
        }
    }
}
