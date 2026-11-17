// Construction menu of a stacking pad: which entries are shown, where, and what selects / validates
// them. Presentation-side logic without any SDL, so it is testable headless. It only reads the game
// and calls pad_build() between ticks, exactly like the AI will.
//
// The menu appears beside the pad while the controlled soldier is near it and the pad offers
// something. Keyboard: SWITCH (Tab) moves the selection, CONFIRM (Enter) builds the selected entry.
// Mouse: clicking an entry builds it (the position is converted to world pixels by the caller).
#pragma once

#include "game/pad.h"

#define PAD_MENU_ENTRY_W 54.0f
#define PAD_MENU_ENTRY_H 18.0f
#define PAD_MENU_GAP 2.0f    // between entries, and between the pad and the list
#define PAD_MENU_MAX_ROWS 6  // entries beyond that start a second column

typedef struct PadMenu {
    bool visible;
    i32 team;
    PadGrid grid;          // snapshot of the crates the options were computed from
    PadOptionList options; // biggest shape first
    i32 selected;          // index in `options`, valid while visible
    Rect rects[PAD_MAX_OPTIONS]; // entry rectangles, world pixels
    // The selection sticks to a (shape, color) across refreshes, not to an index.
    bool has_selection;
    PadShape selected_shape;
    bool selected_blue;
} PadMenu;

// Recomputes visibility, entries and layout from the current game state. Call after each tick.
void pad_menu_refresh(PadMenu *m, const GameState *gs, i32 team);
// Keyboard: SWITCH cycles, CONFIRM builds. Returns true if something was built.
// Call before the tick that consumes `in` (the menu then reflects the crates of the previous tick).
bool pad_menu_handle_input(PadMenu *m, GameState *gs, const InputFrame *in);
// Index of the entry under a world-pixel position, or -1.
i32 pad_menu_hit_test(const PadMenu *m, Vec2 world);
// Mouse: builds the entry under `world` if there is one. Returns true if something was built.
bool pad_menu_click(PadMenu *m, GameState *gs, Vec2 world);
