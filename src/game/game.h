// Simulation entry points. No SDL here: the whole game can run headless.
#pragma once

#include "core/input_frame.h"
#include "core/rng.h"
#include "game/companion.h"
#include "game/level.h"

typedef struct GameState {
    u64 tick;
    Rng rng;
    const Level *level; // not owned
    Companion player;
    u32 respawns; // times the player fell in a hazard (M1: placeholder for real death)
} GameState;

// Returns false if the level has no spawn for team 0.
bool game_init(GameState *gs, u64 seed, const Level *level);
// Advances the simulation by exactly one fixed tick (TICK_DT).
void game_update(GameState *gs, const InputFrame *input);
// Hash of the whole simulation state, to compare runs (determinism tests).
u64 game_hash(const GameState *gs);
