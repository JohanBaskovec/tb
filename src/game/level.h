// Tile map: ASCII level format, parsing and tile queries. No SDL.
//
// File format (.lvl): a header of `key=value` lines (`;` starts a comment line),
// a line containing only `---`, then one text row per tile row (all rows same width).
//
//   .  empty          #  solid            =  one-way platform
//   H  ladder         ~  hazard (water)   1-4 team spawn (empty tile)   D  crate drop zone (empty tile)
//   T  target dummy: an immobile enemy standing on the floor (empty tile)
//   P  stacking pad: the bottom-left cell of a 4x2 grid (4 columns, 2 rows going up; empty tile)
//
// Header keys: `name=...` and `drop.N=COLOR,INTERVAL_S,CAP[,BLUE_PERCENT]` configuring the N-th drop zone
// (N counts the `D` markers in reading order, from 1): COLOR is red, blue or mixed; INTERVAL_S is the delay
// between two drops (seconds); CAP is the number of crates from this zone allowed to exist at once;
// BLUE_PERCENT (mixed only, default 25) is the chance of a blue crate.
//
// Pads are numbered in reading order; `pad.N=TEAM` (1-4) gives the owner of the N-th pad (default: team N).
//
// A ladder cell whose upper neighbour is not a ladder acts as a one-way platform on its top
// (so a ladder can end flush with a floor).
#pragma once

#include "core/arena.h"
#include "core/config.h"
#include "core/geom.h"

#include <math.h>

typedef enum TileType {
    TILE_EMPTY = 0,
    TILE_SOLID,
    TILE_PLATFORM,
    TILE_LADDER,
    TILE_HAZARD,
    TILE_TYPE_COUNT
} TileType;

typedef enum DropColor { DROP_RED, DROP_BLUE, DROP_MIXED } DropColor;

#define DEFAULT_DROP_INTERVAL_S 8
#define DEFAULT_DROP_CAP 3
#define DEFAULT_DROP_BLUE_PERCENT 25

typedef struct DropZoneConfig {
    DropColor color;
    i32 interval_s;
    i32 cap;
    i32 blue_percent;
} DropZoneConfig;

// Where a team's stacking pad is: bottom-left cell of its 4x2 grid.
typedef struct PadSite {
    bool present;
    i32 cell_x, cell_y;
} PadSite;

typedef struct Level {
    i32 width;  // in tiles
    i32 height; // in tiles
    u8 *tiles;  // width * height, row-major; memory owned by the Arena given to level_parse
    // Dynamic solid overlay (resting crates), same layout as `tiles`. Mutated by the game, never by the parser.
    u8 *dyn_solid;
    char name[48];
    Vec2 spawns[MAX_TEAMS]; // bottom-center of the marker cell, in pixels
    bool has_spawn[MAX_TEAMS];
    Vec2 drop_zones[MAX_DROP_ZONES]; // top-center of the marker cell, in pixels
    DropZoneConfig drop_cfg[MAX_DROP_ZONES];
    bool drop_cfg_set[MAX_DROP_ZONES]; // parser bookkeeping
    i32 drop_zone_count;
    Vec2 dummies[MAX_DUMMIES]; // bottom-center of the marker cell, in pixels
    i32 dummy_count;
    PadSite pads[MAX_TEAMS]; // indexed by team
    i32 pad_owner[MAX_TEAMS]; // parser bookkeeping: team of the N-th pad marker (-1 = default)
    i32 pad_marker_count;     // parser bookkeeping
} Level;

// On failure returns false and writes a human-readable message (with line number) in `err`.
bool level_parse(Level *lv, Arena *arena, const char *text, char *err, usize err_len);
bool level_load_file(Level *lv, Arena *arena, const char *path, char *err, usize err_len);

// Outside the map: left/right/top are solid walls, below the bottom is hazard (void).
TileType level_tile(const Level *lv, i32 tx, i32 ty);
// Solid terrain or a resting crate.
bool level_is_solid(const Level *lv, i32 tx, i32 ty);
// Forgets every resting crate (new match on the same level).
void level_clear_dynamic(Level *lv);
void level_set_dyn_solid(Level *lv, i32 tx, i32 ty, bool solid);
bool level_dyn_solid(const Level *lv, i32 tx, i32 ty);
// Thin platform, or the top cell of a ladder: solid only when landing on it from above.
bool level_is_one_way(const Level *lv, i32 tx, i32 ty);

static inline i32 level_px_to_tile(f32 px) {
    return (i32)floorf(px / (f32)TILE_SIZE);
}

TileType level_tile_at_px(const Level *lv, f32 x, f32 y);

static inline f32 level_width_px(const Level *lv) {
    return (f32)(lv->width * TILE_SIZE);
}

static inline f32 level_height_px(const Level *lv) {
    return (f32)(lv->height * TILE_SIZE);
}
