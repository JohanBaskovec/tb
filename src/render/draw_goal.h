// The build-goal ring (middle click) and the little pictures of crate shapes it shares with the HUD.
#pragma once

#include "app/goal_menu.h"
#include "render/render.h"

// A shape drawn in crates (bottom-aligned from `origin`, the top left of its 4x2 area), `cell` pixels per crate. A blue
// version has one blue crate among the red ones.
void draw_goal_shape(Render *r, PadShape shape, bool blue, Vec2 origin, f32 cell);

// `current` (may be NULL) is the team's goal, outlined; `mouse` (logical pixels, may be NULL) highlights what a click
// would choose.
void draw_goal_menu(Render *r, const GoalMenu *menu, const BuildGoal *current, const Vec2 *mouse);
