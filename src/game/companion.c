#include "game/companion.h"

#include "game/tuning.h"

#include <math.h>
#include <string.h>

#define COYOTE_TICKS SECONDS_TO_TICKS(TUNING_COYOTE_S)
#define JUMP_BUFFER_TICKS SECONDS_TO_TICKS(TUNING_JUMP_BUFFER_S)
#define DROP_TICKS SECONDS_TO_TICKS(TUNING_DROP_THROUGH_S)
#define LADDER_COOLDOWN_TICKS SECONDS_TO_TICKS(TUNING_LADDER_COOLDOWN_S)

void companion_init(Companion *c, Vec2 feet) {
    memset(c, 0, sizeof(*c));
    c->body.size = vec2(TUNING_COMPANION_W, TUNING_COMPANION_H);
    c->body.pos = vec2(feet.x - TUNING_COMPANION_W * 0.5f, feet.y - TUNING_COMPANION_H);
    c->prev_pos = c->body.pos;
    c->facing = 1;
    c->aim_dir = vec2(1.0f, 0.0f);
    c->hp = c->max_hp = TUNING_SOLDIER_HP;
    c->weapon = WEAPON_FISTS;
}

bool companion_in_hazard(const Companion *c, const Level *lv) {
    return body_overlaps_tile(&c->body, lv, TILE_HAZARD);
}

// ---- Ladders ---------------------------------------------------------------------------------

static bool ladder_at(const Level *lv, f32 x, f32 y) {
    return level_tile_at_px(lv, x, y) == TILE_LADDER;
}

static bool can_grab_ladder(const Companion *c, const Level *lv, bool up, bool down) {
    const Body *b = &c->body;
    const f32 cx = b->pos.x + b->size.x * 0.5f;
    const f32 mid = b->pos.y + b->size.y * 0.5f;
    const f32 feet = b->pos.y + b->size.y;
    const bool overlapping = ladder_at(lv, cx, mid) || ladder_at(lv, cx, feet - 1.0f);
    if (up && overlapping) {
        return true;
    }
    if (down) {
        // Standing on top of a ladder: grab the cell under the feet. On the ground at the foot of
        // a ladder, down must not re-grab it (that would block walking while holding down).
        return ladder_at(lv, cx, feet + 1.0f) || (!b->on_ground && overlapping);
    }
    return false;
}

static bool still_on_ladder(const Companion *c, const Level *lv) {
    const Body *b = &c->body;
    const f32 cx = b->pos.x + b->size.x * 0.5f;
    const f32 feet = b->pos.y + b->size.y;
    return ladder_at(lv, cx, b->pos.y + b->size.y * 0.5f) || ladder_at(lv, cx, feet - 1.0f) ||
           ladder_at(lv, cx, feet + 1.0f);
}

static void leave_ladder(Companion *c, f32 dir) {
    c->on_ladder = false;
    c->ladder_cooldown_ticks = LADDER_COOLDOWN_TICKS;
    c->coyote_ticks = 0;
    c->jump_buffer_ticks = 0;
    if (dir != 0.0f) { // hop sideways off the ladder
        c->body.vel.y = -TUNING_JUMP_SPEED;
        c->body.vel.x = dir * TUNING_WALK_SPEED;
        c->is_jumping = true;
    } else {
        c->body.vel.y = 0.0f;
    }
}

static void ladder_update(Companion *c, const Level *lv, const InputFrame *in) {
    Body *b = &c->body;

    // Lock onto the ladder column.
    const i32 tx = level_px_to_tile(b->pos.x + b->size.x * 0.5f);
    b->pos.x = ((f32)tx + 0.5f) * (f32)TILE_SIZE - b->size.x * 0.5f;
    b->vel.x = 0.0f;
    b->vel.y = input_axis_y(in) * TUNING_CLIMB_SPEED;

    const f32 dy = b->vel.y * TICK_DT;
    if (dy < 0.0f) {
        // Reaching the top: step onto the floor flush with the top of the ladder.
        const f32 feet = b->pos.y + b->size.y;
        const i32 row = level_px_to_tile(feet - 0.001f);
        if (level_tile(lv, tx, row) == TILE_LADDER) {
            i32 top = row;
            while (level_tile(lv, tx, top - 1) == TILE_LADDER) {
                top--;
            }
            const f32 top_y = (f32)(top * TILE_SIZE);
            if (feet + dy <= top_y) {
                b->pos.y = top_y - b->size.y;
                b->vel.y = 0.0f;
                b->on_ground = true;
                c->on_ladder = false;
                return;
            }
        }
    }

    body_move(b, lv, TICK_DT, true);
    if (b->on_ground || !still_on_ladder(c, lv)) {
        c->on_ladder = false;
    }
}

// ---- Main update -----------------------------------------------------------------------------

void companion_update(Companion *c, const Level *lv, const InputFrame *in) {
    Body *b = &c->body;
    c->prev_pos = b->pos;

    const f32 dir = input_axis_x(in);
    const bool up = input_down(in, ACT_UP);
    const bool down = input_down(in, ACT_DOWN);
    const bool jump_pressed = input_pressed(in, ACT_JUMP);

    // Timers.
    if (c->ladder_cooldown_ticks > 0) {
        c->ladder_cooldown_ticks--;
    }
    if (c->jump_buffer_ticks > 0) {
        c->jump_buffer_ticks--;
    }
    if (jump_pressed) {
        c->jump_buffer_ticks = JUMP_BUFFER_TICKS;
    }
    if (b->on_ground) {
        c->coyote_ticks = COYOTE_TICKS;
    } else if (c->coyote_ticks > 0) {
        c->coyote_ticks--;
    }

    const bool carrying = entity_id_valid(c->carrying);

    // Ladders (not while carrying a crate).
    if (!c->on_ladder && !carrying && c->ladder_cooldown_ticks == 0 && (up || down) &&
        can_grab_ladder(c, lv, up, down)) {
        c->on_ladder = true;
        c->is_jumping = false;
        c->drop_ticks = 0;
    }
    if (c->on_ladder) {
        if (!jump_pressed) {
            ladder_update(c, lv, in);
            return;
        }
        leave_ladder(c, dir);
    }

    // Horizontal movement.
    const f32 walk_speed = carrying ? TUNING_WALK_SPEED * TUNING_CARRY_SPEED_MULT : TUNING_WALK_SPEED;
    const f32 jump_speed = carrying ? TUNING_JUMP_SPEED * TUNING_CARRY_JUMP_MULT : TUNING_JUMP_SPEED;
    const f32 target = dir * walk_speed;
    f32 accel;
    if (dir != 0.0f) {
        accel = b->on_ground ? TUNING_GROUND_ACCEL : TUNING_AIR_ACCEL;
    } else {
        accel = b->on_ground ? TUNING_GROUND_DECEL : TUNING_AIR_DECEL;
    }
    b->vel.x = approachf(b->vel.x, target, accel * TICK_DT);
    if (dir > 0.0f) {
        c->facing = 1;
    } else if (dir < 0.0f) {
        c->facing = -1;
    }

    // Jump or drop through a one-way platform.
    if (jump_pressed && down && b->on_ground && body_standing_on_one_way_only(b, lv)) {
        c->drop_ticks = DROP_TICKS;
        b->vel.y = TUNING_DROP_VY;
        b->on_ground = false;
        c->jump_buffer_ticks = 0;
        c->coyote_ticks = 0;
    } else if (c->jump_buffer_ticks > 0 && c->coyote_ticks > 0) {
        b->vel.y = -jump_speed;
        b->on_ground = false;
        c->is_jumping = true;
        c->jump_buffer_ticks = 0;
        c->coyote_ticks = 0;
    }
    // Releasing jump while rising cuts the jump short (variable height).
    if (c->is_jumping && input_released(in, ACT_JUMP) && b->vel.y < 0.0f) {
        b->vel.y *= TUNING_JUMP_CUT;
    }

    // Gravity and collisions.
    b->vel.y = MIN(b->vel.y + TUNING_GRAVITY * TICK_DT, TUNING_MAX_FALL_SPEED);
    body_move(b, lv, TICK_DT, c->drop_ticks > 0);
    if (c->drop_ticks > 0) {
        c->drop_ticks--;
    }
    if (b->on_ground || b->vel.y >= 0.0f) {
        c->is_jumping = false;
    }
}
