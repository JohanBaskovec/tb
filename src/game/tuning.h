// All gameplay constants live here (speeds in px/s, accelerations in px/s², durations in seconds).
#pragma once

#include "core/config.h"

#define SECONDS_TO_TICKS(s) ((u32)((s) * (f32)TICK_HZ + 0.5f))

// Companion body (hitbox; the sprite is drawn slightly larger).
#define TUNING_COMPANION_W 12.0f
#define TUNING_COMPANION_H 22.0f

// Horizontal movement.
#define TUNING_WALK_SPEED 100.0f
#define TUNING_GROUND_ACCEL 900.0f
#define TUNING_GROUND_DECEL 1200.0f
#define TUNING_AIR_ACCEL 600.0f
#define TUNING_AIR_DECEL 200.0f

// Vertical movement. Jump height = v² / 2g ≈ 50 px (about 3 tiles).
#define TUNING_GRAVITY 900.0f
#define TUNING_MAX_FALL_SPEED 420.0f
#define TUNING_JUMP_SPEED 300.0f
#define TUNING_JUMP_CUT 0.5f // vel.y multiplier when jump is released while rising
#define TUNING_COYOTE_S 0.1f
#define TUNING_JUMP_BUFFER_S 0.1f

// Dropping through one-way platforms (down + jump).
#define TUNING_DROP_THROUGH_S 0.15f
#define TUNING_DROP_VY 60.0f

// Ladders.
#define TUNING_CLIMB_SPEED 70.0f
#define TUNING_LADDER_COOLDOWN_S 0.15f // no re-grab right after letting go

// Crates (16x16 px = one tile).
#define TUNING_CRATE_SIZE 16.0f
#define TUNING_CRATE_DESCENT_SPEED 60.0f // slow descent from the sky, constant
#define TUNING_THROW_VX 150.0f           // lob: horizontal speed
#define TUNING_THROW_VY 250.0f           // lob: initial upward speed
#define TUNING_PICKUP_REACH 16.0f        // how far in front of the body a crate can be grabbed
#define TUNING_PLACE_MAX_RAISE 2         // a crate can be placed up to this many tiles above the feet

// Carrying a crate: slower, and a lower jump. No ladders, no shooting.
#define TUNING_CARRY_SPEED_MULT 0.75f
#define TUNING_CARRY_JUMP_MULT 0.8f

// Combat. Weapons themselves are a data table (game/weapon.c).
#define TUNING_SOLDIER_HP 100         // the base soldier; the other variants arrive with the squad milestone
#define TUNING_DUMMY_HP 100           // target dummies
#define TUNING_DUMMY_W 14.0f
#define TUNING_DUMMY_H 24.0f
#define TUNING_HURT_FLASH_S 0.15f     // a hit soldier / dummy flashes for this long
#define TUNING_PUNCH_ANIM_S 0.12f
#define TUNING_AIM_MIN_DIST 4.0f      // closer than this to the shoulder, the cursor gives no direction
#define TUNING_MUZZLE_HEIGHT 9.0f     // shots leave this far below the top of the body
#define TUNING_EXPLOSION_MIN_FALLOFF 0.4f // damage at the edge of an explosion, as a fraction of the center's
#define TUNING_EXPLOSION_LIFT 60.0f   // extra upward speed given by an explosion at point-blank
#define TUNING_GRENADE_BOUNCE 0.35f   // fraction of the vertical speed kept when a grenade lands
#define TUNING_GRENADE_WALL_BOUNCE 0.4f
#define TUNING_GRENADE_FRICTION 0.7f  // horizontal speed multiplier at each ground contact
#define TUNING_LOOT_W 12.0f           // pad products and weapons lying on the ground
#define TUNING_LOOT_H 12.0f

// Stacking pad menu: shown while the controlled soldier is within this distance of the pad (px,
// measured from the soldier's center to the pad rectangle).
#define TUNING_PAD_MENU_RANGE_X 56.0f
#define TUNING_PAD_MENU_RANGE_Y 40.0f
