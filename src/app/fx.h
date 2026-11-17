// Short-lived visual effects (explosions, sparks, muzzle flashes), built from the game's events.
// Pure presentation, no SDL: the game never reads it, so it cannot change gameplay or the hash.
#pragma once

#include "game/events.h"

#define MAX_FX 64

typedef enum FxKind {
    FX_EXPLOSION, // radius = blast radius
    FX_SPARK,     // something was hit
    FX_MUZZLE,    // a shot left the weapon
} FxKind;

typedef struct Fx {
    FxKind kind;
    Vec2 pos;
    f32 radius;
    u32 age, life; // ticks
} Fx;

typedef struct FxList {
    Fx items[MAX_FX];
    u32 count;
} FxList;

// Turns the events of one tick into effects. Call once per game tick.
void fx_consume(FxList *list, const EventQueue *events);
// Ages the effects by one tick and drops the finished ones.
void fx_step(FxList *list);
// Progress of an effect in [0, 1).
static inline f32 fx_progress(const Fx *fx) {
    return (f32)fx->age / (f32)fx->life;
}
