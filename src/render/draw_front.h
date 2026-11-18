// Draws the screens around a match (title, setup, pause, results) and the match itself.
#pragma once

#include "app/front.h"
#include "render/render.h"

// `crosshair` (logical pixels, may be NULL): drawn while playing. `flags`: DRAW_* of draw_game.
void draw_front(Render *r, const Front *f, const Vec2 *crosshair, f32 alpha, u32 flags);
