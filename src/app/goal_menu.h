// The build-goal menu (middle mouse button): a ring of every shape the pad can make, in red and in blue, around the
// pointer. Choosing one sets the team's build goal (game/goal.h). No SDL.
//
// Entry i is shape i % SHAPE_COUNT, in blue when i >= SHAPE_COUNT: the red shapes fill the right half of the ring
// (from the top, clockwise), the blue ones the left half. Laid out in logical screen pixels, like the debug menu.
#pragma once

#include "game/pad.h"

#define GOAL_MENU_ENTRIES (SHAPE_COUNT * 2)
#define GOAL_MENU_CENTER (-2) // goal_menu_hit: the disc in the middle (clears the goal)

#define GOAL_MENU_RADIUS 96.0f // from the centre to the middle of an entry
#define GOAL_MENU_INNER 36.0f  // radius of the centre disc
#define GOAL_MENU_ENTRY_W 40.0f
#define GOAL_MENU_ENTRY_H 30.0f

typedef struct GoalMenu {
    bool visible;
    Vec2 center; // logical pixels
} GoalMenu;

// Opens the ring around `at`, moved if need be so that all of it is on screen.
void goal_menu_open(GoalMenu *menu, Vec2 at);
void goal_menu_close(GoalMenu *menu);

void goal_menu_entry(i32 index, PadShape *shape, bool *blue);
// The entry showing this goal, for highlighting.
i32 goal_menu_index_of(PadShape shape, bool blue);
Rect goal_menu_entry_box(const GoalMenu *menu, i32 index);
// The entry under a position, GOAL_MENU_CENTER over the middle disc, else -1 (also -1 while hidden).
i32 goal_menu_hit(const GoalMenu *menu, Vec2 logical);

// What a shape looks like in crates, bottom-aligned: the cells (row 0 is the top) of its first drawing. Returns how many.
u32 goal_shape_cells(PadShape shape, PadCoord out[PAD_CELLS]);
