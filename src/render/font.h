// Bitmap font drawn for the project: 5x7 capitals, digits, punctuation, and the accented capitals of French.
// Pure data and text measurement (no SDL): the drawing itself is in render/draw_ui.c.
//
// Text is UTF-8. Lower case letters are drawn as capitals. An accent is drawn in the two rows above the
// glyph (one row of space in between), a cedilla in the two rows under it. Unknown characters show as '?'.
#pragma once

#include "core/types.h"

#define FONT_GLYPH_W 5
#define FONT_GLYPH_H 7
#define FONT_ADVANCE 6     // glyph width + one column of space
#define FONT_ACCENT_ROOM 3 // rows above the glyph used by accents
#define FONT_LINE_H 12     // from one line to the next (accents and cedillas included)

typedef enum FontMark {
    FONT_MARK_NONE,
    FONT_MARK_ACUTE,
    FONT_MARK_GRAVE,
    FONT_MARK_CIRCUMFLEX,
    FONT_MARK_DIAERESIS,
    FONT_MARK_CEDILLA,
} FontMark;

typedef struct FontGlyph {
    const char *pixels; // FONT_GLYPH_W * FONT_GLYPH_H characters, row by row: '#' = lit
    FontMark mark;
} FontGlyph;

// Decodes the code point at *s and moves *s past it. Malformed bytes give U+FFFD (one byte at a time).
// Returns 0 at the end of the string (without moving).
u32 font_next_codepoint(const char **s);
// The glyph of a code point; false (and the '?' glyph) if the font does not have it.
bool font_glyph(u32 codepoint, FontGlyph *out);
// Lit pixels of a mark: 2 rows of FONT_GLYPH_W characters (NULL for FONT_MARK_NONE).
const char *font_mark_pixels(FontMark mark);
// Width of a text in pixels at `scale` (no trailing space column).
i32 font_text_width(const char *text, i32 scale);
