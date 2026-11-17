// Combat: shooting, projectiles, explosions, damage, dummies, death of the soldier. No SDL.
//
// Targets are the soldiers of every team and the dummies (team TEAM_NONE). Bullets and punches only hurt other
// teams; explosions hurt everybody, the shooter included (friendly fire), but not through walls.
// Hit points are only ever lowered here; a soldier whose hit points reach zero dies at the end of
// the tick (see squad.h).
#pragma once

#include "game/game.h"

#define PLAYER_TEAM 0 // the team of the human player
#define TEAM_NONE (-1) // dummies: hostile to every team

// Handles weapon pickup / swap (grab key), then firing (fire key) for one soldier. `crate_busy` is true
// when the grab or fire key was already used by a crate action this tick (carrying, picking up,
// throwing): the soldier then neither shoots nor picks up a weapon.
void combat_soldier_update(GameState *gs, Soldier *soldier, const InputFrame *input, bool crate_busy);
// Projectiles in flight and dummy timers. Call after the soldiers' updates.
void combat_update(GameState *gs);

// Equips the soldier the human controls with a fresh weapon (debug / tests). The old weapon is discarded.
void combat_give_weapon(GameState *gs, WeaponKind kind);
void soldier_give_weapon(Companion *c, WeaponKind kind);
// A soldier is gone: its weapon (with the remaining ammo) lies where it was and its crate falls.
void combat_drop_loot(GameState *gs, Companion *c);
// True if no solid tile (terrain or resting crate) lies on the segment a-b.
bool combat_line_clear(const Level *lv, Vec2 a, Vec2 b);

// Creates an immobile target standing with its feet at `feet`. Returns ENTITY_NONE if the pool is full.
EntityId dummy_spawn(GameState *gs, Vec2 feet, i32 team);
// Creates a projectile of `weapon`, centered on `center`, moving at `vel`. Returns ENTITY_NONE if the pool is full.
EntityId projectile_spawn(GameState *gs, WeaponKind weapon, i32 team, Vec2 center, Vec2 vel);
// Explosion of `weapon` (radius, damage and knockback from its definition) at `center`.
void combat_explode(GameState *gs, Vec2 center, WeaponKind weapon);

// Number of live dummies / projectiles.
u32 dummy_count(const GameState *gs);
u32 projectile_count(const GameState *gs);
