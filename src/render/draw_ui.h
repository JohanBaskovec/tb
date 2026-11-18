// Text and the bits of interface shared by the HUD and the menus: drawn with a Render, in logical pixels.
#pragma once

#include "game/game.h"
#include "game/pad.h"
#include "render/render.h"

typedef enum TextAlign { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT } TextAlign;

// Draws UTF-8 text; (x, y) is the top-left of the capitals (accents go above), or the top-center / top-right.
// Returns the width drawn.
i32 ui_text(Render *r, f32 x, f32 y, const char *text, Color color, i32 scale, TextAlign align);
// Same, with a one-pixel dark shadow (readable over the level).
i32 ui_text_shadow(Render *r, f32 x, f32 y, const char *text, Color color, i32 scale, TextAlign align);
// A dark rounded-looking box with a border.
void ui_panel(Render *r, Rect box, Color border);

// Colors and names shown to the players.
Color ui_team_color(i32 team);
Color ui_team_color_dark(i32 team);
const char *ui_team_name(i32 team);      // "ROUGES", "BLEUS"...
const char *ui_order_label(Order order); // "SUIVRE", "GARDER"...
const char *ui_weapon_label(WeaponKind weapon);
const char *ui_product_label(PadProduct product);
