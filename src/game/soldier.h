// Soldiers: every fighting unit of every team, the human-controlled one included. No SDL.
//
// A soldier is a Companion (movement, weapon, carrying) plus a team, a variant and a bit of AI
// memory. Soldiers that nobody controls are driven by game/ai.c, which produces an ordinary
// InputFrame for them: the simulation treats a human and an AI soldier exactly alike.
#pragma once

#include "core/config.h"
#include "game/companion.h"
#include "game/walker.h"

#define MAX_SOLDIERS 32

typedef enum SoldierVariant {
    SOLDIER_NORMAL,        // 2x1 vertical, red crate
    SOLDIER_STRONG,        // 2x1 vertical with a blue crate
    SOLDIER_HEAVY,         // 2x2, comes with a rocket launcher
    SOLDIER_HEAVY_ARMORED, // 2x2 with a blue crate
    SOLDIER_VARIANT_COUNT
} SoldierVariant;

typedef struct SoldierDef {
    const char *name;
    i32 hp;
    WeaponKind start_weapon; // WEAPON_FISTS = none
} SoldierDef;

const SoldierDef *soldier_def(SoldierVariant variant);

// What a team's soldiers do when nobody controls them (one order for the whole team).
typedef enum Order {
    ORDER_FOLLOW, // stay near the controlled soldier and shoot enemies in range
    ORDER_GUARD,  // stay near the team's pad and shoot enemies in range
    ORDER_COLLECT, // fetch crates and bring them to the pad
    ORDER_ATTACK, // go to the nearest enemy pad and shoot what is in the way
    ORDER_COUNT
} Order;

const char *order_name(Order order);

typedef enum CollectState {
    COLLECT_SEEK,    // looking for a crate
    COLLECT_FETCH,   // walking to the chosen crate
    COLLECT_DELIVER, // carrying a crate to the pad
} CollectState;

// Memory of the AI of one soldier (also part of the state hash: it decides what the soldier does).
typedef struct SoldierAi {
    Walker walker;
    u32 clock;         // ticks since the soldier exists: paces the trigger
    CollectState collect;
    i32 deliver_dir;   // +1: walks in from the left of the pad and fills it from the right; -1: the mirror
    EntityId crate;    // crate being fetched
    u32 state_ticks;   // ticks spent in the current collect state
    EntityId banned;   // crate that could not be fetched: ignored for a while
    u32 banned_ticks;
} SoldierAi;

typedef struct Soldier {
    bool alive; // slot in use
    i32 team;
    SoldierVariant variant;
    Companion c;
    SoldierAi ai;
    InputFrame input; // last frame (AI soldiers): edges are derived from it
} Soldier;

typedef struct Team {
    bool active; // has a spawn in the level
    bool eliminated;
    Order order;
    Vec2 spawn;  // bottom-center, pixels
} Team;
