// Side-scrolling camera: dead zone, look-ahead, smoothing, level bounds. Pure math (no SDL).
#pragma once

#include "game/level.h"

typedef struct Camera {
    Vec2 pos;      // top-left of the view, in world pixels
    Vec2 prev_pos; // previous tick, for interpolation
    f32 view_w, view_h;
    f32 look_x; // current horizontal look-ahead offset
} Camera;

void camera_init(Camera *c, f32 view_w, f32 view_h);
// Follows `target` (world pixels, usually the body center). `facing` is -1/+1 (look-ahead side).
// `snap` jumps straight to the target (first frame, respawn, teleport).
void camera_follow(Camera *c, Vec2 target, i32 facing, const Level *lv, bool snap);
// Moves the camera to an explicit top-left position (clamped to the level).
void camera_set(Camera *c, Vec2 top_left, const Level *lv);
// Render position between the previous and current tick.
Vec2 camera_interpolated(const Camera *c, f32 alpha);
