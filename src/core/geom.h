// Vec2, Rect (AABB) and small scalar helpers. Header-only.
#pragma once

#include "core/types.h"

typedef struct Vec2 {
    f32 x, y;
} Vec2;

// x, y = top-left corner; y grows downward (screen convention).
typedef struct Rect {
    f32 x, y, w, h;
} Rect;

static inline Vec2 vec2(f32 x, f32 y) {
    return (Vec2){x, y};
}

static inline Vec2 vec2_add(Vec2 a, Vec2 b) {
    return (Vec2){a.x + b.x, a.y + b.y};
}

static inline Vec2 vec2_sub(Vec2 a, Vec2 b) {
    return (Vec2){a.x - b.x, a.y - b.y};
}

static inline Vec2 vec2_scale(Vec2 a, f32 s) {
    return (Vec2){a.x * s, a.y * s};
}

static inline f32 clampf(f32 v, f32 lo, f32 hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline f32 lerpf(f32 a, f32 b, f32 t) {
    return a + (b - a) * t;
}

// Moves `cur` toward `target` by at most `step` (step >= 0) without overshooting.
static inline f32 approachf(f32 cur, f32 target, f32 step) {
    if (cur < target) {
        return cur + step < target ? cur + step : target;
    }
    return cur - step > target ? cur - step : target;
}

static inline bool rect_overlap(Rect a, Rect b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static inline bool rect_contains(Rect r, Vec2 p) {
    return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}
