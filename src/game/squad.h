// Squads: spawning, controlling, ordering and removing soldiers; one update for all of them. No SDL.
#pragma once

#include "game/game.h"

// Creates a soldier standing with its feet at `feet`. Returns its index, or -1 if the squad table is
// full or the body would not fit there. Fires EV_SOLDIER_SPAWNED.
i32 soldier_spawn(GameState *gs, i32 team, SoldierVariant variant, Vec2 feet);
// Same, but trusts the spot (the level's spawn): no check that the body fits.
i32 soldier_spawn_at_start(GameState *gs, i32 team, SoldierVariant variant, Vec2 feet);
// The soldier a pad product is, if it is one.
bool soldier_from_product(PadProduct product, SoldierVariant *out);

// Moves, lets carry crates and fight every living soldier for one tick. The controlled one reads `human`,
// the others are driven by ai_think. Call once per tick.
void squad_update(GameState *gs, const InputFrame *human);
// End of the tick: soldiers at 0 hit points die, soldiers in the water go back to their team's spawn.
void squad_resolve(GameState *gs);

// Kills a soldier now: it drops its weapon and crate, the control passes to the closest soldier of
// its team, and the team is eliminated if it was the last one.
void soldier_kill(GameState *gs, i32 index);
// Back to the team's spawn, healthy, with the variant's starting weapon (it fell in the water).
void soldier_respawn(GameState *gs, i32 index);

// Gives the control to the next living soldier of the controlled soldier's team (cycles).
void squad_cycle_control(GameState *gs);
// Next order for the team of the controlled soldier.
void squad_cycle_order(GameState *gs);
