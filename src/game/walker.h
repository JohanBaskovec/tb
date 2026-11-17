// Goal-driven test bot: walks toward a target x and hops over obstacles that block the way.
// Not a pathfinder (that comes with the AI milestone): it only reacts to what is directly ahead.
#pragma once

#include "core/input_frame.h"
#include "game/companion.h"

typedef struct Walker {
    f32 target_x;     // pixels, compared with the center of the body
    u32 jump_hold;    // ticks left holding jump (a full-height hop)
    u32 blocked_ticks; // consecutive ticks pushing against something it cannot hop over
} Walker;

void walker_init(Walker *w, f32 target_x);
void walker_think(Walker *w, const Level *lv, const Companion *c, InputFrame *frame);
bool walker_arrived(const Walker *w, const Companion *c);
// True when stuck against an obstacle for more than about a second.
bool walker_stuck(const Walker *w);
