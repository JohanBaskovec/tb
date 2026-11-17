// Simple soldier AI: one behavior per team order, no pathfinding (navigation comes in M5). No SDL.
//
// The AI only produces an InputFrame, exactly what a keyboard would: it walks with the Walker (which
// hops small obstacles), aims, fires and presses grab like a player. So it can do nothing a human cannot.
#pragma once

#include "game/game.h"

// Fills `out` (this tick's input) for the soldier at `index`, following its team's order.
void ai_think(GameState *gs, i32 index, InputFrame *out);
