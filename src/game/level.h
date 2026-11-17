// Tile map: ASCII level format, parsing and tile queries. No SDL.
//
// File format (.lvl): a header of `key=value` lines (`;` starts a comment line),
// a line containing only `---`, then one text row per tile row (all rows same width).
//
//   .  empty          #  solid            =  one-way platform
//   H  ladder         ~  hazard (water)   1-4 team spawn (empty tile)   D  crate drop zone (empty tile)
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

typedef struct Level {
    i32 width;  // in tiles
    i32 height; // in tiles
    u8 *tiles;  // width * height, row-major; memory owned by the Arena given to level_parse
    char name[48];
    Vec2 spawns[MAX_TEAMS]; // bottom-center of the marker cell, in pixels
    bool has_spawn[MAX_TEAMS];
    Vec2 drop_zones[MAX_DROP_ZONES]; // top-center of the marker cell, in pixels
    i32 drop_zone_count;
} Level;

// On failure returns false and writes a human-readable message (with line number) in `err`.
bool level_parse(Level *lv, Arena *arena, const char *text, char *err, usize err_len);
bool level_load_file(Level *lv, Arena *arena, const char *path, char *err, usize err_len);

// Outside the map: left/right/top are solid walls, below the bottom is hazard (void).
TileType level_tile(const Level *lv, i32 tx, i32 ty);
bool level_is_solid(const Level *lv, i32 tx, i32 ty);
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
