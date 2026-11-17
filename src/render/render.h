// 2D renderer over SDL. In headless mode it draws into an off-screen software surface
// (no window, no display server needed), which makes screenshots possible in CI.
#pragma once

#include "core/geom.h"

typedef struct Color {
    u8 r, g, b, a;
} Color;

typedef struct Render Render;

// Returns NULL on failure. `headless` = software renderer on a LOGICAL_W x LOGICAL_H surface.
Render *render_create(bool headless, const char *title);
void render_destroy(Render *r);

void render_begin(Render *r, Color clear);
void render_end(Render *r);

void render_fill_rect(Render *r, Rect rect, Color c);
void render_outline_rect(Render *r, Rect rect, Color c);
void render_line(Render *r, Vec2 a, Vec2 b, Color c);

// Window pixels (mouse position) -> logical 640x360 pixels, undoing the letterbox and scaling.
Vec2 render_window_to_logical(const Render *r, f32 window_x, f32 window_y);

void render_set_title(Render *r, const char *title);
// Current window title (empty string in headless mode).
const char *render_title(const Render *r);

// Saves what was drawn since render_begin as a PNG. Call before render_end.
bool render_screenshot(Render *r, const char *path);
