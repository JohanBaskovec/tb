// Scripted/random virtual players that produce InputFrames, for headless simulations.
#pragma once

#include "core/input_frame.h"
#include "core/rng.h"

typedef struct Bot {
    Rng rng; // private RNG: never consumes the game's random stream
    u32 mask;
    u32 ticks_left;
    u32 jump_ticks_left;
} Bot;

void bot_init(Bot *bot, u64 seed);
// Fills `frame` with this tick's inputs.
void bot_think(Bot *bot, InputFrame *frame);
