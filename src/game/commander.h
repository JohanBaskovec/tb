// The team commander: what a human does with the pad menu and the order key, done for an AI team. No SDL.
//
// Every half second it looks at the team's situation (soldiers, crates, the pad, enemies close to the base) and
//  - builds on the pad what is worth building (the same pad_build call as the menu),
//  - gives each soldier a role (collect, steal, attack, guard): the soldiers carry them out with game/ai.c.
// It uses only what a player could see, and only the actions a player has.
#pragma once

#include "game/game.h"

// Value the commander puts on a pad product (0 = never builds it).
i32 commander_product_value(PadProduct product);

// One tick for every AI team. Call before the soldiers think.
void commander_update(GameState *gs);
