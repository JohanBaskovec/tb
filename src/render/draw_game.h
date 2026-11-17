// Draws a GameState (read-only) with a Render, through a Camera.
#pragma once

#include "game/game.h"
#include "render/camera.h"
#include "render/render.h"

enum {
    DRAW_DEBUG = 1u << 0, // tile grid, hitboxes, velocity, state flags, spawns, drop zones
};

// `alpha` in [0,1]: interpolation between the previous and current tick.
void draw_game(Render *r, const GameState *gs, const Camera *cam, f32 alpha, u32 flags);
