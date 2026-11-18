#include "game/weapon.h"

#include "game/tuning.h"

// Placeholder balance (see GAME_DESIGN.md § 7): to be tuned by playing.
static const WeaponDef WEAPONS[WEAPON_COUNT] = {
    [WEAPON_FISTS] = {.name = "fists",
                      .shot = SHOT_MELEE,
                      .max_ammo = 0,
                      .cooldown_ticks = SECONDS_TO_TICKS(0.25f),
                      .damage = 8,
                      .reach = 14.0f,
                      .knockback = 90.0f},
    [WEAPON_PISTOL] = {.name = "pistol",
                       .shot = SHOT_BULLET,
                       .max_ammo = 24,
                       .cooldown_ticks = SECONDS_TO_TICKS(0.2f),
                       .speed = 360.0f,
                       .damage = 12,
                       .knockback = 60.0f,
                       .life_ticks = SECONDS_TO_TICKS(0.9f),
                       .proj_w = 4.0f,
                       .proj_h = 3.0f},
    [WEAPON_UZI] = {.name = "uzi",
                    .shot = SHOT_BULLET,
                    .max_ammo = 60,
                    .cooldown_ticks = 5,
                    .speed = 380.0f,
                    .spread = 0.08f,
                    .damage = 6,
                    .knockback = 25.0f,
                    .life_ticks = SECONDS_TO_TICKS(0.7f),
                    .proj_w = 4.0f,
                    .proj_h = 3.0f},
    [WEAPON_GRENADES] = {.name = "grenades",
                         .shot = SHOT_GRENADE,
                         .max_ammo = 6,
                         .cooldown_ticks = SECONDS_TO_TICKS(0.5f),
                         .speed = 170.0f,
                         .lift = 200.0f,
                         .damage = 60,
                         .radius = 40.0f,
                         .knockback = 220.0f,
                         .life_ticks = SECONDS_TO_TICKS(1.5f),
                         .proj_w = 6.0f,
                         .proj_h = 6.0f},
    [WEAPON_STRONG_GRENADES] = {.name = "strong grenades",
                                .shot = SHOT_GRENADE,
                                .max_ammo = 4,
                                .cooldown_ticks = SECONDS_TO_TICKS(0.5f),
                                .speed = 170.0f,
                                .lift = 200.0f,
                                .damage = 100,
                                .radius = 64.0f,
                                .knockback = 300.0f,
                                .life_ticks = SECONDS_TO_TICKS(1.5f),
                                .proj_w = 6.0f,
                                .proj_h = 6.0f},
    [WEAPON_ROCKET_LAUNCHER] = {.name = "rocket launcher",
                                .shot = SHOT_ROCKET,
                                .max_ammo = 5,
                                .cooldown_ticks = SECONDS_TO_TICKS(0.55f),
                                .speed = 220.0f,
                                .damage = 90,
                                .radius = 48.0f,
                                .knockback = 260.0f,
                                .life_ticks = SECONDS_TO_TICKS(2.0f),
                                .proj_w = 8.0f,
                                .proj_h = 4.0f},
};

const WeaponDef *weapon_def(WeaponKind kind) {
    return &WEAPONS[(kind >= 0 && kind < WEAPON_COUNT) ? kind : WEAPON_FISTS];
}

const char *weapon_name(WeaponKind kind) {
    return weapon_def(kind)->name;
}
