// Soldiers: every fighting unit of every team, the human-controlled one included. No SDL.
//
// A soldier is a Companion (movement, weapon, carrying) plus a team, a variant and a bit of AI
// memory. Soldiers that nobody controls are driven by game/ai.c, which produces an ordinary
// InputFrame for them: the simulation treats a human and an AI soldier exactly alike.
#pragma once

#include "core/config.h"
#include "game/companion.h"
#include "game/nav.h"
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
    ORDER_STEAL,  // take crates from the enemy pads and bring them to ours
    ORDER_COUNT
} Order;

const char *order_name(Order order);

// What the AI of a soldier is doing right now (decided every tick; shown by the debug overlay).
typedef enum AiMode {
    AI_IDLE,
    AI_FOLLOW,  // staying close to the controlled soldier
    AI_GUARD,   // holding the post at the pad
    AI_FETCH,   // on the way to a crate
    AI_DELIVER, // carrying a crate to the pad
    AI_STEAL,   // on the way to a crate of an enemy pad
    AI_ATTACK,  // advancing on an enemy base
    AI_ARM,     // going for a weapon lying around
    AI_FLEE,    // hurt, with an enemy close: retreating to the base
    AI_MODE_COUNT
} AiMode;

const char *ai_mode_name(AiMode mode);

typedef enum CollectState {
    COLLECT_SEEK,    // looking for a crate
    COLLECT_FETCH,   // walking to the chosen crate
    COLLECT_DELIVER, // carrying a crate to the pad
} CollectState;

// Memory of the AI of one soldier (also part of the state hash: it decides what the soldier does).
#define AI_BAN_SLOTS 4

typedef struct SoldierAi {
    Walker walker;     // fine positioning (a few pixels), and the fallback when there is no navigation graph
    NavAgent nav;      // the trip to the current goal
    Order role;        // what the team's commander told this soldier to do (teams run by the AI)
    AiMode mode;
    u32 clock;         // ticks since the soldier exists: paces the trigger
    CollectState collect;
    i32 deliver_dir;   // +1: walks in from the left of the pad and fills it from the right; -1: the mirror
    i32 deliver_col;   // pad column this soldier is bringing its crate to under a build goal (-1: none yet)
    EntityId crate;    // crate being fetched
    u32 state_ticks;   // ticks spent in the current collect state
    EntityId banned[AI_BAN_SLOTS]; // crates that could not be fetched or delivered: ignored for a while
    u32 banned_ticks[AI_BAN_SLOTS];
} SoldierAi;

typedef struct Soldier {
    bool alive; // slot in use
    i32 team;
    SoldierVariant variant;
    Companion c;
    SoldierAi ai;
    InputFrame input; // last frame (AI soldiers): edges are derived from it
} Soldier;

// What a team did during the match (results screen).
typedef struct TeamStats {
    u32 kills;       // enemy soldiers it finished off
    u32 losses;      // soldiers it lost (the water does not count: they come back)
    u32 built;       // pad constructions
    u32 base_damage; // hit points it took from enemy bases
} TeamStats;

// What the team's allies try to build on the pad while the order is "collect" (game/goal.h). `shape` is a PadShape.
typedef struct BuildGoal {
    bool active;
    i32 shape;
    bool blue; // the blue version (at least one blue crate); the red version also accepts blue crates
} BuildGoal;

typedef struct Team {
    bool active; // has a spawn in the level
    bool playing; // took part in the match: had at least one soldier
    bool eliminated;
    bool ai;     // run by a commander (game/commander.c): builds, gives roles; no human behind it
    Order order;
    Vec2 spawn;  // bottom-center, pixels
    bool has_base; // a team with a spawn and a pad has a base (game/base.h)
    i32 base_hp;   // 0 once destroyed: the pad cannot be used any more
    u32 base_hurt_ticks; // > 0: the base was just hit (flash; the commander defends)
    u32 stagnant_ticks; // commander: how long the pad has been waiting without a new crate
    u32 pad_crates;     // commander: crates on the pad when it last looked
    BuildGoal goal;     // chosen by the human with the middle-click menu
    TeamStats stats;
} Team;
