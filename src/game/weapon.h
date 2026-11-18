// Weapon definitions: a data table, so adding a weapon means adding a row. No SDL.
//
// A weapon is one of four ways of shooting (ShotKind); the numbers of the row say how fast, how
// far, how hard. Fists are the fallback weapon: infinite, short-range, weak.
#pragma once

#include "core/types.h"

typedef enum WeaponKind {
    WEAPON_FISTS, // "unarmed"; also what a soldier holds when its weapon runs dry
    WEAPON_PISTOL,
    WEAPON_UZI,
    WEAPON_GRENADES,
    WEAPON_STRONG_GRENADES,
    WEAPON_ROCKET_LAUNCHER,
    WEAPON_COUNT
} WeaponKind;

typedef enum ShotKind {
    SHOT_MELEE,   // hits whatever is within reach in front of the soldier
    SHOT_BULLET,  // straight, not affected by gravity, disappears on impact
    SHOT_GRENADE, // lobbed: gravity, bounces, explodes when its fuse runs out or on contact
    SHOT_ROCKET,  // straight, explodes on impact
} ShotKind;

typedef struct WeaponDef {
    const char *name;
    ShotKind shot;
    i32 max_ammo;       // shots in a fresh weapon, 0 = infinite
    u32 cooldown_ticks; // minimum delay between two shots (holding the key repeats them)
    f32 speed;          // projectile speed (grenade: horizontal speed), px/s
    f32 lift;           // grenade: initial upward speed, px/s
    f32 spread;         // bullets: random vertical speed, as a fraction of `speed` (0 = perfectly straight)
    i32 damage;         // bullet / punch damage; explosions: damage at the center
    f32 radius;         // explosion radius, px (0 = no explosion)
    f32 reach;          // melee: reach in front of the body, px
    f32 knockback;      // impulse given to what is hit, px/s
    u32 life_ticks;     // bullet / rocket range, grenade fuse
    f32 proj_w, proj_h; // projectile size, px
} WeaponDef;

const WeaponDef *weapon_def(WeaponKind kind);
const char *weapon_name(WeaponKind kind);
