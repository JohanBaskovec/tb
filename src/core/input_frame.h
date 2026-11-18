// Platform-neutral input for one player and one tick. Produced by the platform layer
// (keyboard/gamepad) or by bots/AI; consumed by the simulation.
#pragma once

#include "core/geom.h"
#include "core/types.h"

typedef enum Action {
    ACT_LEFT,
    ACT_RIGHT,
    ACT_UP,
    ACT_DOWN,
    ACT_JUMP,
    ACT_FIRE,
    ACT_GRAB,
    ACT_SWITCH,
    ACT_ORDER,
    ACT_PAUSE,
    ACT_CONFIRM, // validates the selected entry of a menu (pad construction menu)
    ACT_DESTROY, // breaks the crate in front (or the carried one) for hearts or ammunition (right mouse button)
    ACT_COUNT
} Action;

typedef struct InputFrame {
    u32 down;     // bit per Action currently held
    u32 pressed;  // went down this tick
    u32 released; // went up this tick
    // Mouse aiming: when active, `aim` is the point the soldier aims at, in world pixels. The sources
    // of input (mouse in the window loop, script) set it; input_frame_update never touches it.
    bool aim_active;
    Vec2 aim;
} InputFrame;

static inline u32 action_bit(Action a) {
    return 1u << (u32)a;
}

// Feeds the held-actions mask for the new tick and derives pressed/released edges.
static inline void input_frame_update(InputFrame *f, u32 down_mask) {
    f->pressed = down_mask & ~f->down;
    f->released = f->down & ~down_mask;
    f->down = down_mask;
}

static inline bool input_down(const InputFrame *f, Action a) {
    return (f->down & action_bit(a)) != 0;
}

static inline bool input_pressed(const InputFrame *f, Action a) {
    return (f->pressed & action_bit(a)) != 0;
}

static inline bool input_released(const InputFrame *f, Action a) {
    return (f->released & action_bit(a)) != 0;
}

static inline f32 input_axis_x(const InputFrame *f) {
    return (f32)input_down(f, ACT_RIGHT) - (f32)input_down(f, ACT_LEFT);
}

static inline f32 input_axis_y(const InputFrame *f) {
    return (f32)input_down(f, ACT_DOWN) - (f32)input_down(f, ACT_UP);
}
