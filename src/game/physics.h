// AABB-vs-tilemap movement. Axis-by-axis resolution (X then Y). No SDL.
#pragma once

#include "game/level.h"

typedef struct Body {
    Vec2 pos;  // top-left corner of the box, in pixels
    Vec2 vel;  // px/s
    Vec2 size;
    bool on_ground;   // landed on something during the last move
    bool hit_wall;
    bool hit_ceiling;
} Body;

static inline Rect body_rect(const Body *b) {
    return (Rect){b->pos.x, b->pos.y, b->size.x, b->size.y};
}

static inline Vec2 body_center(const Body *b) {
    return vec2(b->pos.x + b->size.x * 0.5f, b->pos.y + b->size.y * 0.5f);
}

// Moves by vel * dt. A single step must stay under one tile (asserted).
// One-way tiles only stop a body that was entirely above them, unless `ignore_one_way`.
void body_move(Body *b, const Level *lv, f32 dt, bool ignore_one_way);

// True if the tiles right under the feet are all one-way (so the body can drop through).
bool body_standing_on_one_way_only(const Body *b, const Level *lv);

// True if the body (shrunk by 1px) overlaps a tile of this type.
bool body_overlaps_tile(const Body *b, const Level *lv, TileType type);
