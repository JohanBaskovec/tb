// Simulation entry points. No SDL here: the whole game can run headless.
#pragma once

#include "core/input_frame.h"
#include "core/rng.h"
#include "game/companion.h"
#include "game/entity.h"
#include "game/events.h"
#include "game/level.h"
#include "game/nav.h"
#include "game/soldier.h"

// Counters of what happened to crates since the start (tests, future HUD and stats screen).
typedef struct CrateStats {
    u32 spawned, settled, picked_up, placed, thrown, destroyed, consumed;
} CrateStats;

// Counters of what happened in fights since the start (tests, future HUD and stats screen).
typedef struct CombatStats {
    u32 shots, hits, explosions, kills, deaths;
} CombatStats;

// Outcome of the match (game/rules.h).
typedef struct MatchResult {
    bool over;
    i32 winner;   // team index, -1 for a draw (only meaningful once over)
    u64 end_tick; // tick at which it ended
} MatchResult;

typedef struct GameState {
    u64 tick;
    Rng rng;
    Level *level; // not owned; mutable because resting crates are written into its solid overlay
    Soldier soldiers[MAX_SOLDIERS];
    Team teams[MAX_TEAMS];
    i32 controlled;  // index of the soldier the human controls, -1 if none (the team is gone)
    Companion ghost; // stands in for the controlled soldier once none is left: dead, where the last one fell
    u32 respawns; // times a soldier fell in a hazard and came back at its team's spawn
    EntityPool entities;
    u32 drop_timers[MAX_DROP_ZONES]; // ticks until the next drop of each zone
    CrateStats crate_stats;
    u32 products_built;
    CombatStats combat_stats;
    MatchResult match;
    Nav nav;           // graph of the level for the AI; built on first use, derived from the level only (not hashed)
    EventQueue events; // what happened during the last tick (presentation only, not hashed)
} GameState;

// The soldier the human controls (a dead "ghost" when its whole team is gone, never NULL).
Companion *game_player(GameState *gs);
const Companion *game_player_const(const GameState *gs);
// Number of living soldiers of a team.
u32 team_soldier_count(const GameState *gs, i32 team);

// Returns false if the level has no spawn for team 0.
bool game_init(GameState *gs, u64 seed, Level *level);
// Puts a team under (or takes it from) the command of the AI: a commander builds on its pad and gives its soldiers
// roles. For the human's team (index 0) this also takes the control away: all its soldiers are driven by the AI.
// A team with a spawn and nobody alive gets a first soldier.
void game_set_team_ai(GameState *gs, i32 team, bool ai);
// Advances the simulation by exactly one fixed tick (TICK_DT).
void game_update(GameState *gs, const InputFrame *input);
// Hash of the whole simulation state, to compare runs (determinism tests).
u64 game_hash(const GameState *gs);
