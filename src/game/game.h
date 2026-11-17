// Simulation entry points. No SDL here: the whole game can run headless.
#pragma once

#include "core/input_frame.h"
#include "core/rng.h"
#include "game/companion.h"
#include "game/entity.h"
#include "game/events.h"
#include "game/level.h"

// Counters of what happened to crates since the start (tests, future HUD and stats screen).
typedef struct CrateStats {
    u32 spawned, settled, picked_up, placed, thrown, destroyed, consumed;
} CrateStats;

// Counters of what happened in fights since the start (tests, future HUD and stats screen).
typedef struct CombatStats {
    u32 shots, hits, explosions, kills, deaths;
} CombatStats;

typedef struct GameState {
    u64 tick;
    Rng rng;
    Level *level; // not owned; mutable because resting crates are written into its solid overlay
    Companion player;
    u32 respawns; // times the player fell in a hazard (M1: placeholder for real death)
    EntityPool entities;
    u32 drop_timers[MAX_DROP_ZONES]; // ticks until the next drop of each zone
    CrateStats crate_stats;
    u32 products_built;
    CombatStats combat_stats;
    EventQueue events; // what happened during the last tick (presentation only, not hashed)
} GameState;

// Returns false if the level has no spawn for team 0.
bool game_init(GameState *gs, u64 seed, Level *level);
// Advances the simulation by exactly one fixed tick (TICK_DT).
void game_update(GameState *gs, const InputFrame *input);
// Hash of the whole simulation state, to compare runs (determinism tests).
u64 game_hash(const GameState *gs);
