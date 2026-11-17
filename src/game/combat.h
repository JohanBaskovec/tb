// Combat: shooting, projectiles, explosions, damage, dummies, death of the soldier. No SDL.
//
// Targets are the player's soldier (team 0) and the dummies. Bullets and punches only hurt other
// teams; explosions hurt everybody, the shooter included (friendly fire), but not through walls.
// Hit points are only ever lowered here; a soldier whose hit points reach zero dies at the end of
// the tick (see game_update).
#pragma once

#include "game/game.h"

#define PLAYER_TEAM 0

// Handles weapon pickup / swap (grab key), then firing (fire key) for the player. `crate_busy` is true
// when the grab or fire key was already used by a crate action this tick (carrying, picking up,
// throwing): the soldier then neither shoots nor picks up a weapon.
void combat_player_update(GameState *gs, const InputFrame *input, bool crate_busy);
// Projectiles in flight and dummy timers. Call after the player's update.
void combat_update(GameState *gs);

// Equips the player with a fresh weapon (debug / tests / the AI later). The old weapon is discarded.
void combat_give_weapon(GameState *gs, WeaponKind kind);
// The player's soldier died: drops its weapon (if any) and its crate on the spot. The caller respawns it.
void combat_player_died(GameState *gs);

// Creates an immobile target standing with its feet at `feet`. Returns ENTITY_NONE if the pool is full.
EntityId dummy_spawn(GameState *gs, Vec2 feet, i32 team);
// Creates a projectile of `weapon`, centered on `center`, moving at `vel`. Returns ENTITY_NONE if the pool is full.
EntityId projectile_spawn(GameState *gs, WeaponKind weapon, i32 team, Vec2 center, Vec2 vel);
// Explosion of `weapon` (radius, damage and knockback from its definition) at `center`.
void combat_explode(GameState *gs, Vec2 center, WeaponKind weapon);

// Number of live dummies / projectiles.
u32 dummy_count(const GameState *gs);
u32 projectile_count(const GameState *gs);
