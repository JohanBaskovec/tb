#include "game/walker.h"

#include <math.h>

#define ARRIVE_DIST 3.0f
#define HOP_TICKS 22 // jump held long enough for the full ~50 px
#define STUCK_TICKS (TICK_HZ)

void walker_init(Walker *w, f32 target_x) {
    w->target_x = target_x;
    w->jump_hold = 0;
    w->blocked_ticks = 0;
}

bool walker_arrived(const Walker *w, const Companion *c) {
    return fabsf(w->target_x - (c->body.pos.x + c->body.size.x * 0.5f)) <= ARRIVE_DIST;
}

bool walker_stuck(const Walker *w) {
    return w->blocked_ticks > STUCK_TICKS;
}

static bool solid_ahead(const Level *lv, const Body *b, f32 dir, f32 height_above_feet) {
    const f32 x = (dir > 0.0f ? b->pos.x + b->size.x : b->pos.x) + dir * 3.0f;
    // Terrain or a resting crate.
    return level_is_solid(lv, level_px_to_tile(x),
                          level_px_to_tile(b->pos.y + b->size.y - height_above_feet));
}

void walker_think(Walker *w, const Level *lv, const Companion *c, InputFrame *frame) {
    const Body *b = &c->body;
    const f32 dx = w->target_x - (b->pos.x + b->size.x * 0.5f);
    u32 mask = 0;

    f32 dir = 0.0f;
    if (fabsf(dx) > ARRIVE_DIST) {
        dir = dx > 0.0f ? 1.0f : -1.0f;
        mask |= action_bit(dir > 0.0f ? ACT_RIGHT : ACT_LEFT);
    }

    if (w->jump_hold > 0) {
        w->jump_hold--;
        mask |= action_bit(ACT_JUMP);
    } else if (dir != 0.0f && b->on_ground && solid_ahead(lv, b, dir, 4.0f)) {
        // Something at foot level: hop. Tall walls are retried, then reported as "stuck".
        w->jump_hold = HOP_TICKS;
        mask |= action_bit(ACT_JUMP);
    }

    if (dir != 0.0f && b->hit_wall) {
        w->blocked_ticks++;
    } else if (b->on_ground && !b->hit_wall) {
        w->blocked_ticks = 0;
    }
    input_frame_update(frame, mask);
}
