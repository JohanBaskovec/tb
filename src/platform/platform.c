#include "platform/platform.h"

#include "core/log.h"

#include <SDL3/SDL.h>

bool platform_init(bool headless) {
    SDL_InitFlags flags = headless ? SDL_INIT_EVENTS : (SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    if (!SDL_Init(flags)) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

void platform_shutdown(void) {
    SDL_Quit();
}

const char *platform_video_driver(void) {
    const char *name = SDL_GetCurrentVideoDriver();
    return name ? name : "";
}

void platform_pump_events(PlatformEvents *out) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            out->quit = true;
        } else if (e.type == SDL_EVENT_MOUSE_MOTION) {
            out->mouse_moved = true;
        } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            out->mouse_moved = true;
            out->click = true;
            out->click_x = e.button.x;
            out->click_y = e.button.y;
        } else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
            switch (e.key.scancode) {
            case SDL_SCANCODE_F1:
                out->toggle_debug = true;
                break;
            case SDL_SCANCODE_F2:
                out->toggle_pause = true;
                break;
            case SDL_SCANCODE_F3:
                out->step = true;
                break;
            case SDL_SCANCODE_F4:
                out->toggle_slow = true;
                break;
            default:
                break;
            }
        }
    }
}

static void map_key(const bool *keys, int count, int scancode, Action action, u32 *mask) {
    if (scancode < count && keys[scancode]) {
        *mask |= action_bit(action);
    }
}

// Keys are identified by SDL *scancodes*, i.e. by their physical position on the keyboard, not by
// the letter printed on them. The four movement keys are the ones at the WASD position: they read
// WASD on a QWERTY keyboard and ZQSD on an AZERTY one, and they are the same physical keys on every
// layout. (Using keycodes would make "Z" mean a different key on each layout.)
u32 platform_map_keyboard(const bool *keys, int count) {
    u32 mask = 0;
    map_key(keys, count, SDL_SCANCODE_LEFT, ACT_LEFT, &mask);
    map_key(keys, count, SDL_SCANCODE_RIGHT, ACT_RIGHT, &mask);
    map_key(keys, count, SDL_SCANCODE_UP, ACT_UP, &mask);
    map_key(keys, count, SDL_SCANCODE_DOWN, ACT_DOWN, &mask);
    map_key(keys, count, SDL_SCANCODE_A, ACT_LEFT, &mask);  // Q on AZERTY
    map_key(keys, count, SDL_SCANCODE_D, ACT_RIGHT, &mask); // D on AZERTY
    map_key(keys, count, SDL_SCANCODE_W, ACT_UP, &mask);    // Z on AZERTY
    map_key(keys, count, SDL_SCANCODE_S, ACT_DOWN, &mask);  // S on AZERTY
    map_key(keys, count, SDL_SCANCODE_SPACE, ACT_JUMP, &mask);
    map_key(keys, count, SDL_SCANCODE_X, ACT_FIRE, &mask);
    map_key(keys, count, SDL_SCANCODE_C, ACT_GRAB, &mask);
    map_key(keys, count, SDL_SCANCODE_TAB, ACT_SWITCH, &mask);
    map_key(keys, count, SDL_SCANCODE_Q, ACT_ORDER, &mask);
    map_key(keys, count, SDL_SCANCODE_ESCAPE, ACT_PAUSE, &mask);
    map_key(keys, count, SDL_SCANCODE_RETURN, ACT_CONFIRM, &mask);
    map_key(keys, count, SDL_SCANCODE_KP_ENTER, ACT_CONFIRM, &mask);
    return mask;
}

void platform_read_input(InputFrame *frame) {
    int count = 0;
    const bool *keys = SDL_GetKeyboardState(&count);
    input_frame_update(frame, platform_map_keyboard(keys, count));
}

void platform_read_mouse(f32 *x, f32 *y, bool *left_down) {
    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(x, y);
    *left_down = (buttons & SDL_BUTTON_LMASK) != 0;
}

f64 platform_time_s(void) {
    return (f64)SDL_GetPerformanceCounter() / (f64)SDL_GetPerformanceFrequency();
}

void platform_sleep_ms(u32 ms) {
    SDL_Delay(ms);
}
