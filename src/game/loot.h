// Things lying on the ground that fall and wait: weapons to pick up and pad products that are not
// weapons yet (soldiers, vehicles). No SDL.
#pragma once

#include "game/game.h"

// The weapon a pad product is, if it is one.
bool weapon_from_product(PadProduct product, WeaponKind *out);

// Creates a weapon pickup. `pos` is the top-left corner. Returns ENTITY_NONE if the pool is full.
EntityId loot_spawn_weapon(GameState *gs, WeaponKind kind, i32 ammo, Vec2 pos);

// One tick of physics for every pickup and product. A crate that settles on top of one would bury
// it: it is lifted onto the top of the pile. Falling in the water destroys it.
void loot_update(GameState *gs);
