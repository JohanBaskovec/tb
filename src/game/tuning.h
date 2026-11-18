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
#define TUNING_HP_NORMAL 100          // soldier variants (GAME_DESIGN § 4)
#define TUNING_HP_STRONG 150
#define TUNING_HP_HEAVY 200
#define TUNING_HP_HEAVY_ARMORED 300   // provisional: "even more than the heavy one"
#define TUNING_SOLDIER_HP TUNING_HP_NORMAL
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

// AI soldiers. They travel over the navigation graph (game/nav.c) and press keys like a human.
#define TUNING_AI_RANGE 200.0f        // sees and shoots enemies this close
#define TUNING_AI_RANGE_Y 90.0f       // ... and no more than this far above / below
#define TUNING_AI_CHASE_RANGE 120.0f  // an unarmed soldier runs at an enemy this close
#define TUNING_AI_WORKER_CHASE_RANGE 40.0f // ... but one that collects or steals only when the enemy is this close
#define TUNING_AI_WEAPON_RANGE 140.0f // an unarmed soldier walks to a weapon this close
#define TUNING_AI_FOLLOW_BAND 40.0f   // follows the leader but stops within this distance
#define TUNING_AI_GUARD_BAND 40.0f    // stays within this distance of its post
#define TUNING_AI_BAN_S 20.0f         // a crate that could not be fetched is ignored for this long
#define TUNING_AI_GIVE_UP_S 30.0f     // longest a fetch / delivery may take (trips can now cross the level)
#define TUNING_AI_FLEE_HP_PERCENT 30  // a soldier below this share of its hit points runs from enemies
#define TUNING_AI_FLEE_RANGE 110.0f   // ... when one is this close
#define TUNING_AI_ARM_RANGE_Y 60.0f   // a weapon this much higher / lower is still worth a trip

// Team commander (AI teams): looks at the situation every COMMANDER_PERIOD ticks.
#define TUNING_COMMANDER_PERIOD_S 0.5f
#define TUNING_COMMANDER_MIN_SQUAD 3  // below this, any soldier will do; above, it waits for something better
#define TUNING_COMMANDER_MAX_SQUAD 8  // no more soldiers than this per team
#define TUNING_COMMANDER_PATIENCE_S 15.0f // a pad that waits this long without a new crate builds what it can
#define TUNING_COMMANDER_ATTACK_SQUAD 4   // from this many soldiers on, a part of the team goes on the offensive
#define TUNING_COMMANDER_THREAT_RANGE 220.0f // an enemy this close to the pad: everybody defends

// Stacking pad menu: shown while the controlled soldier is within this distance of the pad (px,
// measured from the soldier's center to the pad rectangle).
#define TUNING_PAD_MENU_RANGE_X 56.0f
#define TUNING_PAD_MENU_RANGE_Y 40.0f
