// Draws a GameState (read-only) with a Render, through a Camera.
#pragma once

#include "app/pad_menu.h"
#include "game/game.h"
#include "render/camera.h"
#include "render/render.h"

enum {
    DRAW_DEBUG = 1u << 0, // tile grid, hitboxes, velocity, state flags, spawns, drop zones
};

// `alpha` in [0,1]: interpolation between the previous and current tick. `menu` may be NULL.
// There is no text rendering yet: menu entries show the shape (with the real crate colors) and a
// colored swatch for the product.
void draw_game(Render *r, const GameState *gs, const Camera *cam, const PadMenu *menu, f32 alpha, u32 flags);
