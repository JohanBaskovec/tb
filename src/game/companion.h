// A soldier: movement, jumping, ladders. No SDL.
#pragma once

#include "core/input_frame.h"
#include "game/entity.h"
#include "game/physics.h"

typedef struct Companion {
    Body body;
    Vec2 prev_pos; // position at the start of the last update, for render interpolation
    i32 facing;    // -1 left, +1 right
    bool on_ladder;
    bool is_jumping; // rising from a jump (enables the variable-height cut)
    u32 coyote_ticks;
    u32 jump_buffer_ticks;
    u32 drop_ticks;            // > 0: ignoring one-way platforms
    u32 ladder_cooldown_ticks; // > 0: cannot grab a ladder
    EntityId carrying;         // crate held above the head, ENTITY_NONE if none
    Vec2 aim_dir;       // unit vector the weapon points along: the cursor if aiming, else the facing
    i32 hp, max_hp;
    WeaponKind weapon;  // WEAPON_FISTS when unarmed
    i32 ammo;           // shots left (meaningless for fists)
    u32 fire_cooldown;  // ticks until the next shot is allowed
    u32 hurt_ticks;     // > 0: flashing after a hit
    u32 punch_ticks;    // > 0: fist extended (presentation)
} Companion;

// `feet` = bottom-center of the body.
void companion_init(Companion *c, Vec2 feet);
void companion_update(Companion *c, const Level *lv, const InputFrame *input);
bool companion_in_hazard(const Companion *c, const Level *lv);
