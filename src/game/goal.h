// The build goal: a shape (and colour) the human picks from the middle-click menu. While the team's order is "collect",
// its allies bring crates to the pad so as to make that shape, then build it. No SDL.
//
//  - A red goal accepts the red version first, but a blue crate already on the pad, or being carried, may be used (the
//    result is then the blue version of the shape). A blue goal needs at least one blue crate.
//  - The soldiers fetch only what is missing (crates already carried or fetched by a mate count), prefer the colour the
//    goal asks for, and put each crate in a column of the pad where the shape needs one.
//  - Once the shape is on the pad (however it got there), an ally builds it and the goal is done.
#pragma once

#include "game/pad.h"

// What the pad needs to get closer to the goal of a team.
typedef struct GoalPlan {
    bool valid;        // the team has an active goal and a usable pad
    bool buildable;    // the shape is on the pad now (in an acceptable colour)
    bool build_blue;   // ... build it in blue
    bool feasible;     // some position of the shape can still be completed (the crates in the way do not forbid it)
    i32 side;          // the side of the pad to bring them from (+1: from the left, -1: from the right)
    u32 missing;       // crates still to bring to the best position of the shape (0 when buildable)
    bool needs_blue;   // one of them must be blue (blue goal, and no blue crate in that position yet)
    u8 filled[PAD_COLS]; // crates in each column of the pad
    u8 wanted[PAD_COLS]; // per column: bit L (0 = bottom crate, 1 = the one above) set when that level is to be filled
} GoalPlan;

void goal_set(GameState *gs, i32 team, PadShape shape, bool blue);
void goal_clear(GameState *gs, i32 team);

// `entry` is the side the soldier comes from (+1 left, -1 right, 0: do not care). A crate is put in a column from the
// column before it, so every column between the soldier and the target one must be empty (the soldiers do not climb
// over a stack of two): positions of the shape that cannot be reached that way are ignored. When nothing can be reached
// from that side, the other side is tried if the pad is low enough (one crate high) to walk around it.
void goal_plan(const GameState *gs, i32 team, i32 entry, GoalPlan *out);

// Builds the goal of every team that has one when its shape is on the pad, the order is collect, and an ally (a soldier
// nobody controls) is there to do it. Call each tick before the soldiers think.
void goal_update(GameState *gs);
