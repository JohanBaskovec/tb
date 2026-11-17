// The stacking pad: a 4x2 grid of cells at the base. Crates resting in it are turned into things
// through a construction menu. No SDL, no automatic construction: the menu is the only trigger,
// for humans and for the AI alike (both end up calling pad_build).
//
// Pure part: PadGrid -> pad_options(). World part: pad_read_grid() / pad_build().
//
// Grid orientation: row 0 is the TOP row, row PAD_ROWS-1 the bottom one (same as on screen).
#pragma once

#include "game/game.h"

#define PAD_COLS 4
#define PAD_ROWS 2
#define PAD_CELLS (PAD_COLS * PAD_ROWS)

typedef enum PadCell { PAD_EMPTY = 0, PAD_RED = 1, PAD_BLUE = 2 } PadCell;

typedef struct PadGrid {
    u8 cells[PAD_ROWS][PAD_COLS]; // PadCell
} PadGrid;

typedef enum PadShape {
    SHAPE_1X1,
    SHAPE_2X1_H, // two side by side
    SHAPE_2X1_V, // two stacked
    SHAPE_L,     // two side by side on the bottom + one on top of either of them
    SHAPE_2X2,
    SHAPE_4X2, // the whole pad
    SHAPE_COUNT
} PadShape;

typedef struct PadCoord {
    u8 col, row;
} PadCoord;

// One entry of the menu: a shape found among the crates, in its red or blue version.
typedef struct PadOption {
    PadShape shape;
    bool blue;         // at least one consumed crate is blue
    PadProduct product;
    u32 count;         // number of crates consumed
    PadCoord cells[PAD_CELLS]; // consumed cells, topmost row first then left to right
} PadOption;

#define PAD_MAX_OPTIONS (SHAPE_COUNT * 2)

typedef struct PadOptionList {
    PadOption items[PAD_MAX_OPTIONS];
    u32 count;
} PadOptionList;

// ---- Pure rules --------------------------------------------------------------------------------

// Every construction the crates of `grid` allow, once per (shape, red/blue), biggest shape first,
// red before blue. For each entry the crates are chosen top first (lowest row index), then leftmost.
// A shape is offered as soon as it is present among the crates, even if there are others.
// A non-empty grid always yields at least a 1x1; an empty grid yields nothing.
void pad_options(const PadGrid *grid, PadOptionList *out);
// Recipe table lookup.
PadProduct pad_recipe(PadShape shape, bool blue);
// Number of crates in a shape (1, 2, 2, 3, 4, 8).
u32 pad_shape_size(PadShape shape);
const char *pad_shape_name(PadShape shape);
const char *pad_product_name(PadProduct product);

// ---- World -------------------------------------------------------------------------------------

// Reads the resting crates of the pad of `team`. False if the team has no pad.
// `ids` (optional) receives the crate entity of each non-empty cell.
bool pad_read_grid(const GameState *gs, i32 team, PadGrid *grid, EntityId ids[PAD_ROWS][PAD_COLS]);
// Options for the pad of `team`; empty if there is no pad, or it cannot be used.
void pad_world_options(const GameState *gs, i32 team, PadOptionList *out);
// Rectangle covered by the pad cells, in pixels.
Rect pad_rect(const GameState *gs, i32 team);
// True if the pad exists and its owner can still use it (a destroyed base cannot: M3).
bool pad_usable(const GameState *gs, i32 team);

// Builds `shape` (red or blue version): consumes the chosen crates and spawns the product next to
// the pad. Re-checks against the current crates, so a stale menu entry is harmlessly refused.
// Returns false (and changes nothing) if the pad is unusable, the shape is no longer there, or
// there is no room for the product.
bool pad_build(GameState *gs, i32 team, PadShape shape, bool blue);

// Test / debug helper: puts resting crates in the pad of `team`. Rows are 4 characters, top row first:
// '.' empty, 'r' red, 'b' blue. Returns false if the team has no pad, a row is malformed, or a cell
// is not free. (A crate with nothing under it falls on the next tick.)
bool pad_debug_fill(GameState *gs, i32 team, const char *top, const char *bottom);
