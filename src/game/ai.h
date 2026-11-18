// Soldier AI: a mode decided every tick (flee, fetch a weapon, rush an enemy, or what the order / role says), carried
// out by travelling over the navigation graph (game/nav.h). No SDL.
//
// The AI only produces an InputFrame, exactly what a keyboard would: it walks, jumps, climbs, aims, fires and presses
// grab like a player. So it can do nothing a human cannot. Teams run by the AI (Team.ai) take their orders from the
// team commander (game/commander.h): each soldier reads its own `ai.role`.
#pragma once

#include "game/game.h"

// Fills `out` (this tick's input) for the soldier at `index`.
void ai_think(GameState *gs, i32 index, InputFrame *out);
