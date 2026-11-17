// The only place that talks to the OS for events, input and time.
#pragma once

#include "core/input_frame.h"

// Things the window loop cares about besides gameplay input.
typedef struct PlatformEvents {
    bool quit;
    bool toggle_debug; // F1
    bool toggle_pause; // F2
    bool step;         // F3: advance one tick while paused
    bool toggle_slow;  // F4: quarter speed
    bool restart;      // F5: start the match over
    bool mouse_moved;  // the mouse moved or clicked: from then on the soldier aims with it
    bool click;        // left mouse button pressed
    f32 click_x, click_y; // where, in window pixels (convert with render_window_to_logical)
} PlatformEvents;

// Headless mode initializes no video/audio subsystem at all.
bool platform_init(bool headless);
void platform_shutdown(void);
// Name of the active SDL video driver ("x11", "wayland", "dummy"...), or "" when none.
const char *platform_video_driver(void);

// Pumps OS events into `out` (flags are only ever set, never cleared: reset them yourself).
void platform_pump_events(PlatformEvents *out);
// Reads the keyboard into this tick's InputFrame (player 0).
void platform_read_input(InputFrame *frame);
// Current mouse position in window pixels and whether the left button is held.
void platform_read_mouse(f32 *x, f32 *y, bool *left_down);
// Keyboard layout. `keys` is indexed by SDL_Scancode with `count` entries. Pure, so it can be tested.
u32 platform_map_keyboard(const bool *keys, int count);
// Monotonic time in seconds.
f64 platform_time_s(void);
void platform_sleep_ms(u32 ms);
