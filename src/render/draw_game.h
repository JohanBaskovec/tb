// Draws a GameState (read-only) with a Render, through a Camera.
#pragma once

#include "app/fx.h"
#include "app/pad_menu.h"
#include "game/game.h"
#include "render/camera.h"
#include "render/render.h"

enum {
    DRAW_DEBUG = 1u << 0, // tile grid, hitboxes, velocity, state flags, spawns, drop zones
    DRAW_NAV = 1u << 1,   // navigation graph, the path of every AI soldier and what it is doing
};

// `alpha` in [0,1]: interpolation between the previous and current tick. `menu`, `fx` and `crosshair`
// (logical pixels, where to draw the aiming cursor) may be NULL.
// There is no text rendering yet: menu entries show the shape (with the real crate colors) and a
// colored swatch for the product; hit points and ammunition are bars.
void draw_game(Render *r, const GameState *gs, const Camera *cam, const PadMenu *menu, const FxList *fx,
               const Vec2 *crosshair, f32 alpha, u32 flags);
