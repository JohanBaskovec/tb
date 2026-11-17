#include "test.h"

#include "platform/platform.h"

#include <SDL3/SDL.h>
#include <string.h>

static void test_keyboard_mapping(void) {
    bool keys[SDL_SCANCODE_COUNT];
    memset(keys, 0, sizeof(keys));
    CHECK_EQ_U64(platform_map_keyboard(keys, SDL_SCANCODE_COUNT), 0);

    keys[SDL_SCANCODE_LEFT] = true;
    keys[SDL_SCANCODE_SPACE] = true;
    u32 m = platform_map_keyboard(keys, SDL_SCANCODE_COUNT);
    CHECK_EQ_U64(m, action_bit(ACT_LEFT) | action_bit(ACT_JUMP));

    memset(keys, 0, sizeof(keys));
    keys[SDL_SCANCODE_RIGHT] = keys[SDL_SCANCODE_UP] = keys[SDL_SCANCODE_DOWN] = true;
    keys[SDL_SCANCODE_X] = keys[SDL_SCANCODE_C] = keys[SDL_SCANCODE_TAB] = true;
    keys[SDL_SCANCODE_Q] = keys[SDL_SCANCODE_ESCAPE] = true;
    keys[SDL_SCANCODE_RETURN] = true;
    m = platform_map_keyboard(keys, SDL_SCANCODE_COUNT);
    u32 expected = 0;
    for (int a = 0; a < ACT_COUNT; a++) {
        expected |= action_bit((Action)a);
    }
    expected &= ~action_bit(ACT_LEFT) & ~action_bit(ACT_JUMP);
    CHECK_EQ_U64(m, expected);

    // A short key array must not be read out of bounds: only scancodes below `count` are read.
    // keys currently holds RIGHT(79) UP(82) DOWN(81) X C TAB Q ESC; with count = 80 (= LEFT's
    // scancode) UP and DOWN are out of range, everything else is still read.
    CHECK_EQ_U64(platform_map_keyboard(keys, 0), 0);
    u32 short_mask = platform_map_keyboard(keys, SDL_SCANCODE_LEFT);
    CHECK_EQ_U64(short_mask, action_bit(ACT_RIGHT) | action_bit(ACT_FIRE) | action_bit(ACT_GRAB) |
                                 action_bit(ACT_SWITCH) | action_bit(ACT_ORDER) |
                                 action_bit(ACT_PAUSE) | action_bit(ACT_CONFIRM));
}

// The movement keys are physical positions (SDL scancodes): the WASD position, which is labelled
// ZQSD on an AZERTY keyboard. Same keys on every layout; the arrow keys keep working too.
static void test_wasd_position_keys(void) {
    static const struct {
        SDL_Scancode sc;
        Action action;
    } cases[] = {{SDL_SCANCODE_W, ACT_UP},
                 {SDL_SCANCODE_A, ACT_LEFT},
                 {SDL_SCANCODE_S, ACT_DOWN},
                 {SDL_SCANCODE_D, ACT_RIGHT},
                 {SDL_SCANCODE_UP, ACT_UP},
                 {SDL_SCANCODE_LEFT, ACT_LEFT},
                 {SDL_SCANCODE_DOWN, ACT_DOWN},
                 {SDL_SCANCODE_RIGHT, ACT_RIGHT}};
    bool keys[SDL_SCANCODE_COUNT];
    for (usize i = 0; i < ARRAY_LEN(cases); i++) {
        memset(keys, 0, sizeof(keys));
        keys[cases[i].sc] = true;
        CHECK_EQ_U64(platform_map_keyboard(keys, SDL_SCANCODE_COUNT), action_bit(cases[i].action));
    }

    // Letter and arrow for the same direction: one action, not two.
    memset(keys, 0, sizeof(keys));
    keys[SDL_SCANCODE_A] = keys[SDL_SCANCODE_LEFT] = true;
    CHECK_EQ_U64(platform_map_keyboard(keys, SDL_SCANCODE_COUNT), action_bit(ACT_LEFT));

    // Diagonal with letters: left + up.
    memset(keys, 0, sizeof(keys));
    keys[SDL_SCANCODE_A] = keys[SDL_SCANCODE_W] = true;
    CHECK_EQ_U64(platform_map_keyboard(keys, SDL_SCANCODE_COUNT),
                 action_bit(ACT_LEFT) | action_bit(ACT_UP));

    // The other letters did not change meaning: Q (AZERTY "A") is still the order menu, not left;
    // X, C and Tab are untouched; the keys next to the movement ones do nothing.
    memset(keys, 0, sizeof(keys));
    keys[SDL_SCANCODE_Q] = true;
    CHECK_EQ_U64(platform_map_keyboard(keys, SDL_SCANCODE_COUNT), action_bit(ACT_ORDER));
    memset(keys, 0, sizeof(keys));
    keys[SDL_SCANCODE_E] = keys[SDL_SCANCODE_R] = keys[SDL_SCANCODE_F] = keys[SDL_SCANCODE_Z] = true;
    CHECK_EQ_U64(platform_map_keyboard(keys, SDL_SCANCODE_COUNT), 0);
}

static void push_key(SDL_Scancode sc, bool repeat) {
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.scancode = sc;
    e.key.repeat = repeat;
    CHECK(SDL_PushEvent(&e));
}

static void test_events(void) {
    CHECK(platform_init(true));
    CHECK(strcmp(platform_video_driver(), "") == 0); // headless: no video subsystem at all

    PlatformEvents ev = {0};
    platform_pump_events(&ev);
    CHECK(!ev.quit && !ev.toggle_debug && !ev.toggle_pause && !ev.step && !ev.toggle_slow);

    push_key(SDL_SCANCODE_F1, false);
    push_key(SDL_SCANCODE_F3, false);
    platform_pump_events(&ev);
    CHECK(ev.toggle_debug && ev.step && !ev.toggle_pause && !ev.toggle_slow && !ev.quit);

    memset(&ev, 0, sizeof(ev));
    push_key(SDL_SCANCODE_F2, false);
    push_key(SDL_SCANCODE_F4, false);
    push_key(SDL_SCANCODE_F1, true); // auto-repeat must be ignored
    platform_pump_events(&ev);
    CHECK(ev.toggle_pause && ev.toggle_slow && !ev.toggle_debug);

    // A left click is reported with its window position; other buttons are ignored.
    memset(&ev, 0, sizeof(ev));
    SDL_Event click;
    memset(&click, 0, sizeof(click));
    click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    click.button.button = SDL_BUTTON_RIGHT;
    click.button.x = 5.0f;
    click.button.y = 6.0f;
    CHECK(SDL_PushEvent(&click));
    platform_pump_events(&ev);
    CHECK(!ev.click);
    click.button.button = SDL_BUTTON_LEFT;
    click.button.x = 123.0f;
    click.button.y = 45.0f;
    CHECK(SDL_PushEvent(&click));
    platform_pump_events(&ev);
    CHECK(ev.click);
    CHECK_NEAR(ev.click_x, 123.0, 0.0);
    CHECK_NEAR(ev.click_y, 45.0, 0.0);

    memset(&ev, 0, sizeof(ev));
    SDL_Event quit;
    memset(&quit, 0, sizeof(quit));
    quit.type = SDL_EVENT_QUIT;
    CHECK(SDL_PushEvent(&quit));
    platform_pump_events(&ev);
    CHECK(ev.quit);

    CHECK(platform_time_s() > 0.0);
    platform_shutdown();
}

int main(void) {
    RUN_TEST(test_keyboard_mapping);
    RUN_TEST(test_wasd_position_keys);
    RUN_TEST(test_events);
    TEST_MAIN_END();
}
