// Game -> presentation event queue. The simulation pushes events as things happen; sound, particles,
// screen shake and UI read them. Nothing in the simulation ever reads them back, so presentation
// code cannot change gameplay (and events are not part of the state hash).
//
// The queue holds the events of the last game_update only: it is cleared at the start of each tick.
// A consumer that can run several ticks per frame must therefore drain it after every tick.
#pragma once

#include "core/geom.h"

typedef enum EventType {
    EV_CRATE_SPAWNED,   // pos = crate position, a = CrateKind, b = drop zone
    EV_CRATE_SETTLED,   // pos = cell position, a = CrateKind
    EV_CRATE_PICKED_UP, // pos, a = CrateKind
    EV_CRATE_PLACED,    // pos, a = CrateKind
    EV_CRATE_THROWN,    // pos, a = CrateKind
    EV_CRATE_DESTROYED, // pos, a = CrateKind (lost in the water, crushed...)
    EV_CRATE_CONSUMED,  // pos, a = CrateKind (used by a pad construction)
    EV_PAD_BUILT,       // pos = pad center, a = PadProduct, b = team
    EV_PLAYER_RESPAWNED,
    EV_SHOT,             // pos = muzzle, a = WeaponKind, b = facing (-1 / +1)
    EV_PUNCH,            // pos = fist, a = number of targets hit
    EV_PROJECTILE_HIT,   // a bullet ended (impact or end of range); pos, a = WeaponKind
    EV_EXPLOSION,        // pos = center, a = radius (px), b = WeaponKind
    EV_DAMAGE,           // pos = target center, a = amount, b = 1 if the target is a soldier, 0 for a dummy
    EV_DUMMY_DESTROYED,  // pos
    EV_PLAYER_DIED,      // pos
    EV_WEAPON_PICKED_UP, // pos, a = WeaponKind
    EV_WEAPON_DROPPED,   // pos, a = WeaponKind (swapped for another, or dropped on death)
    EV_WEAPON_EMPTY,     // pos: the last shot was fired, the soldier is back to fists
    EV_TYPE_COUNT
} EventType;

typedef struct GameEvent {
    EventType type;
    Vec2 pos;
    i32 a, b;
} GameEvent;

#define MAX_EVENTS 128

typedef struct EventQueue {
    GameEvent items[MAX_EVENTS];
    u32 count;
    u32 dropped; // events lost to a full queue during this tick
} EventQueue;

static inline void events_clear(EventQueue *q) {
    q->count = 0;
    q->dropped = 0;
}

static inline void events_push(EventQueue *q, EventType type, Vec2 pos, i32 a, i32 b) {
    if (q->count >= MAX_EVENTS) {
        q->dropped++;
        return;
    }
    q->items[q->count++] = (GameEvent){type, pos, a, b};
}

// Number of events of a type currently in the queue.
static inline u32 events_count_of(const EventQueue *q, EventType type) {
    u32 n = 0;
    for (u32 i = 0; i < q->count; i++) {
        n += q->items[i].type == type ? 1u : 0u;
    }
    return n;
}
