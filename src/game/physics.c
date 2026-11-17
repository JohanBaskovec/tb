#include "game/physics.h"

#include "core/log.h"

#include <math.h>

// Small tolerance so a box resting exactly on a tile edge is not counted as overlapping it.
#define EDGE_EPS 0.001f
// A one-way tile blocks only if the feet were at most this far below its top before the move.
#define ONE_WAY_EPS 0.01f

static i32 tile_of(f32 px) {
    return level_px_to_tile(px);
}

static void move_x(Body *b, const Level *lv, f32 dx) {
    b->hit_wall = false;
    if (dx == 0.0f) {
        return;
    }
    b->pos.x += dx;
    i32 ty0 = tile_of(b->pos.y + EDGE_EPS);
    i32 ty1 = tile_of(b->pos.y + b->size.y - EDGE_EPS);
    if (dx > 0.0f) {
        i32 tx = tile_of(b->pos.x + b->size.x - EDGE_EPS);
        for (i32 ty = ty0; ty <= ty1; ty++) {
            if (level_is_solid(lv, tx, ty)) {
                b->pos.x = (f32)(tx * TILE_SIZE) - b->size.x;
                b->vel.x = 0.0f;
                b->hit_wall = true;
                return;
            }
        }
    } else {
        i32 tx = tile_of(b->pos.x);
        for (i32 ty = ty0; ty <= ty1; ty++) {
            if (level_is_solid(lv, tx, ty)) {
                b->pos.x = (f32)((tx + 1) * TILE_SIZE);
                b->vel.x = 0.0f;
                b->hit_wall = true;
                return;
            }
        }
    }
}

static void move_y(Body *b, const Level *lv, f32 dy, bool ignore_one_way) {
    b->on_ground = false;
    b->hit_ceiling = false;
    if (dy == 0.0f) {
        return;
    }
    const f32 prev_bottom = b->pos.y + b->size.y;
    b->pos.y += dy;
    i32 tx0 = tile_of(b->pos.x + EDGE_EPS);
    i32 tx1 = tile_of(b->pos.x + b->size.x - EDGE_EPS);
    if (dy > 0.0f) {
        i32 ty = tile_of(b->pos.y + b->size.y - EDGE_EPS);
        for (i32 tx = tx0; tx <= tx1; tx++) {
            bool blocks = level_is_solid(lv, tx, ty);
            if (!blocks && !ignore_one_way && level_is_one_way(lv, tx, ty)) {
                blocks = prev_bottom <= (f32)(ty * TILE_SIZE) + ONE_WAY_EPS;
            }
            if (blocks) {
                b->pos.y = (f32)(ty * TILE_SIZE) - b->size.y;
                b->vel.y = 0.0f;
                b->on_ground = true;
                return;
            }
        }
    } else {
        i32 ty = tile_of(b->pos.y);
        for (i32 tx = tx0; tx <= tx1; tx++) {
            if (level_is_solid(lv, tx, ty)) {
                b->pos.y = (f32)((ty + 1) * TILE_SIZE);
                b->vel.y = 0.0f;
                b->hit_ceiling = true;
                return;
            }
        }
    }
}

void body_move(Body *b, const Level *lv, f32 dt, bool ignore_one_way) {
    f32 dx = b->vel.x * dt;
    f32 dy = b->vel.y * dt;
    ASSERT(fabsf(dx) < (f32)TILE_SIZE && fabsf(dy) < (f32)TILE_SIZE);
    move_x(b, lv, dx);
    move_y(b, lv, dy, ignore_one_way);
}

bool body_standing_on_one_way_only(const Body *b, const Level *lv) {
    i32 ty = tile_of(b->pos.y + b->size.y + 0.5f);
    i32 tx0 = tile_of(b->pos.x + EDGE_EPS);
    i32 tx1 = tile_of(b->pos.x + b->size.x - EDGE_EPS);
    bool any_one_way = false;
    for (i32 tx = tx0; tx <= tx1; tx++) {
        if (level_is_solid(lv, tx, ty)) {
            return false;
        }
        any_one_way = any_one_way || level_is_one_way(lv, tx, ty);
    }
    return any_one_way;
}

bool body_overlaps_tile(const Body *b, const Level *lv, TileType type) {
    i32 tx0 = tile_of(b->pos.x + 1.0f);
    i32 tx1 = tile_of(b->pos.x + b->size.x - 1.0f);
    i32 ty0 = tile_of(b->pos.y + 1.0f);
    i32 ty1 = tile_of(b->pos.y + b->size.y - 1.0f);
    for (i32 ty = ty0; ty <= ty1; ty++) {
        for (i32 tx = tx0; tx <= tx1; tx++) {
            if (level_tile(lv, tx, ty) == type) {
                return true;
            }
        }
    }
    return false;
}
