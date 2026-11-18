// Crates: drop zones, slow descent, falling, stacking in the tile grid, carrying. No SDL.
//
// Resting crates are locked in grid cells and are solid for everybody (they are written into the
// Level's dynamic solid overlay, so the physics treats them like terrain). Falling crates are free
// bodies; when they land they snap to a free, supported cell. A resting crate that loses its
// support (the crate below was picked up or destroyed) starts falling again.
#pragma once

#include "game/game.h"

// Creates a crate. `pos` is the top-left corner. A RESTING crate must be aligned on a free cell.
// Returns ENTITY_NONE if the pool is full.
EntityId crate_spawn(GameState *gs, CrateKind kind, Vec2 pos, CrateState state, i32 origin_zone);
// Deferred: the entity is flushed at the end of the tick, but the grid and the carrier are updated now.
void crate_destroy(GameState *gs, EntityId id);
// Same, for a crate used up by a pad construction (counted and announced differently).
void crate_consume(GameState *gs, EntityId id);

// Puts the crates each zone starts with (level `start.N`) on the ground under the zone, side by side.
// Call once, from game_init.
void crates_place_start(GameState *gs);
// One tick of drop zones and crate physics. Call after the companions have moved.
void crates_update(GameState *gs);
// Handles the grab key (pick up / put down) and the fire key (throw) of one soldier.
void crate_carry_update(GameState *gs, Companion *soldier, const InputFrame *input);
// Puts every carried crate back above its carrier's head. Call after the soldiers have moved.
void crate_carried_follow(GameState *gs);

// The crate carried by this soldier (if any) is let go where it is: it falls (the soldier died or was hit).
void crate_release_carried(GameState *gs, Companion *soldier);

// Number of crates alive (not about to be destroyed).
u32 crate_count(const GameState *gs);
