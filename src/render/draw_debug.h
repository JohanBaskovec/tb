// The F2 debug menu, and the short message shown when a key changes a setting (music). Drawn last, over everything.
#pragma once

#include "app/debug_menu.h"
#include "render/render.h"

// `mouse` (logical pixels, may be NULL) highlights the entry under the pointer.
void draw_debug_menu(Render *r, const DebugMenu *menu, const Vec2 *mouse);
// A line of text in a small box near the top of the screen: "MUSIQUE : OUI".
void draw_toast(Render *r, const char *text);
