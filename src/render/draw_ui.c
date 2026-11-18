#include "render/draw_ui.h"

#include "render/font.h"

static const Color TEAM_COLORS[MAX_TEAMS] = {
    {230, 90, 70, 255}, {80, 150, 235, 255}, {100, 200, 110, 255}, {235, 205, 70, 255}};
static const Color TEAM_COLORS_DARK[MAX_TEAMS] = {
    {150, 50, 40, 255}, {40, 90, 160, 255}, {50, 130, 60, 255}, {160, 130, 30, 255}};
static const char *const TEAM_NAMES[MAX_TEAMS] = {"ROUGES", "BLEUS", "VERTS", "JAUNES"};

static const char *const ORDER_LABELS[ORDER_COUNT] = {
    [ORDER_FOLLOW] = "SUIVRE", [ORDER_GUARD] = "GARDER", [ORDER_COLLECT] = "RAMASSER",
    [ORDER_ATTACK] = "ATTAQUER", [ORDER_STEAL] = "VOLER"};

static const char *const WEAPON_LABELS[WEAPON_COUNT] = {
    [WEAPON_FISTS] = "POINGS",           [WEAPON_PISTOL] = "PISTOLET",
    [WEAPON_UZI] = "UZI",                [WEAPON_GRENADES] = "GRENADES",
    [WEAPON_STRONG_GRENADES] = "GRENADES+", [WEAPON_ROCKET_LAUNCHER] = "LANCE-ROQUETTES"};

static const char *const PRODUCT_LABELS[PRODUCT_COUNT] = {
    [PRODUCT_PISTOL] = "PISTOLET",
    [PRODUCT_UZI] = "UZI",
    [PRODUCT_GRENADES] = "GRENADES",
    [PRODUCT_STRONG_GRENADES] = "GRENADES+",
    [PRODUCT_COMPANION] = "SOLDAT",
    [PRODUCT_STRONG_COMPANION] = "SOLDAT FORT",
    [PRODUCT_ROCKET_LAUNCHER] = "LANCE-ROQUETTES",
    [PRODUCT_HEAVY_COMPANION] = "SOLDAT LOURD",
    [PRODUCT_HEAVY_ARMORED_COMPANION] = "LOURD BLINDÉ",
    [PRODUCT_TANK] = "TANK",
    [PRODUCT_HEAVY_TANK] = "TANK LOURD",
};

static void draw_pixels(Render *r, f32 x, f32 y, const char *pixels, i32 rows, Color color, i32 scale) {
    const f32 s = (f32)scale;
    for (i32 row = 0; row < rows; row++) {
        for (i32 col = 0; col < FONT_GLYPH_W; col++) {
            if (pixels[row * FONT_GLYPH_W + col] == '#') {
                render_fill_rect(r, (Rect){x + (f32)col * s, y + (f32)row * s, s, s}, color);
            }
        }
    }
}

i32 ui_text(Render *r, f32 x, f32 y, const char *text, Color color, i32 scale, TextAlign align) {
    const i32 width = font_text_width(text, scale);
    if (align == ALIGN_CENTER) {
        x -= (f32)(width / 2);
    } else if (align == ALIGN_RIGHT) {
        x -= (f32)width;
    }
    const f32 s = (f32)scale;
    u32 cp;
    while ((cp = font_next_codepoint(&text)) != 0) {
        FontGlyph g;
        font_glyph(cp, &g);
        draw_pixels(r, x, y, g.pixels, FONT_GLYPH_H, color, scale);
        const char *mark = font_mark_pixels(g.mark);
        if (mark) {
            const f32 my = g.mark == FONT_MARK_CEDILLA ? y + (f32)FONT_GLYPH_H * s : y - (f32)FONT_ACCENT_ROOM * s;
            draw_pixels(r, x, my, mark, 2, color, scale);
        }
        x += (f32)(FONT_ADVANCE * scale);
    }
    return width;
}

i32 ui_text_shadow(Render *r, f32 x, f32 y, const char *text, Color color, i32 scale, TextAlign align) {
    const Color shadow = {12, 12, 20, color.a};
    ui_text(r, x + (f32)scale, y + (f32)scale, text, shadow, scale, align);
    return ui_text(r, x, y, text, color, scale, align);
}

void ui_panel(Render *r, Rect box, Color border) {
    render_fill_rect(r, box, (Color){18, 20, 34, 225});
    render_fill_rect(r, (Rect){box.x + 1.0f, box.y, box.w - 2.0f, 1.0f}, border);
    render_fill_rect(r, (Rect){box.x + 1.0f, box.y + box.h - 1.0f, box.w - 2.0f, 1.0f}, border);
    render_fill_rect(r, (Rect){box.x, box.y + 1.0f, 1.0f, box.h - 2.0f}, border);
    render_fill_rect(r, (Rect){box.x + box.w - 1.0f, box.y + 1.0f, 1.0f, box.h - 2.0f}, border);
}

Color ui_team_color(i32 team) {
    return team >= 0 && team < MAX_TEAMS ? TEAM_COLORS[team] : (Color){200, 200, 200, 255};
}

Color ui_team_color_dark(i32 team) {
    return team >= 0 && team < MAX_TEAMS ? TEAM_COLORS_DARK[team] : (Color){110, 110, 110, 255};
}

const char *ui_team_name(i32 team) {
    return team >= 0 && team < MAX_TEAMS ? TEAM_NAMES[team] : "?";
}

const char *ui_order_label(Order order) {
    return order >= 0 && order < ORDER_COUNT ? ORDER_LABELS[order] : "?";
}

const char *ui_weapon_label(WeaponKind weapon) {
    return weapon >= 0 && weapon < WEAPON_COUNT ? WEAPON_LABELS[weapon] : "?";
}

const char *ui_product_label(PadProduct product) {
    return product >= 0 && product < PRODUCT_COUNT ? PRODUCT_LABELS[product] : "?";
}
