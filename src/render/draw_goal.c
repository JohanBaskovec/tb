#include "render/draw_goal.h"

#include "core/config.h"
#include "render/draw_game.h"
#include "render/draw_ui.h"
#include "render/font.h"

#include <math.h>

static const Color COL_CRATE_RED = {208, 64, 56, 255};
static const Color COL_CRATE_BLUE = {56, 112, 226, 255};
static const Color COL_CRATE_EDGE = {60, 34, 22, 255};
static const Color COL_BG = {20, 22, 36, 240};
static const Color COL_BORDER = {120, 126, 160, 255};
static const Color COL_GOLD = {255, 224, 80, 255};
static const Color COL_TEXT = {236, 236, 244, 255};
static const Color COL_DIM = {150, 154, 176, 255};

void draw_goal_shape(Render *r, PadShape shape, bool blue, Vec2 origin, f32 cell) {
    PadCoord cells[PAD_CELLS];
    const u32 n = goal_shape_cells(shape, cells);
    for (u32 i = 0; i < n; i++) {
        const Rect box = {origin.x + (f32)cells[i].col * cell, origin.y + (f32)cells[i].row * cell, cell, cell};
        render_fill_rect(r, box, blue && i == 0 ? COL_CRATE_BLUE : COL_CRATE_RED);
        render_outline_rect(r, box, COL_CRATE_EDGE);
    }
}

static void fill_disc(Render *r, Vec2 c, f32 radius, Color color) {
    for (f32 dy = -radius; dy <= radius; dy += 1.0f) {
        const f32 half = sqrtf(MAX(radius * radius - dy * dy, 0.0f));
        render_fill_rect(r, (Rect){c.x - half, c.y + dy, half * 2.0f, 1.0f}, color);
    }
}

static void draw_entry(Render *r, const Rect box, i32 index, bool hovered, bool current) {
    PadShape shape;
    bool blue;
    goal_menu_entry(index, &shape, &blue);
    render_fill_rect(r, box, hovered ? (Color){52, 58, 104, 245} : COL_BG);
    render_outline_rect(r, box, hovered || current ? COL_GOLD : COL_BORDER);
    if (hovered) {
        render_outline_rect(r, (Rect){box.x + 1.0f, box.y + 1.0f, box.w - 2.0f, box.h - 2.0f}, COL_GOLD);
    }
    const f32 cell = 5.0f;
    PadCoord cells[PAD_CELLS];
    const u32 n = goal_shape_cells(shape, cells);
    u8 widest = 0;
    for (u32 i = 0; i < n; i++) {
        widest = MAX(widest, (u8)(cells[i].col + 1));
    }
    draw_goal_shape(r, shape, blue, vec2(roundf(box.x + (box.w - (f32)widest * cell) * 0.5f), box.y + 3.0f), cell);
    draw_product_icon(r, pad_recipe(shape, blue), 0, vec2(box.x + box.w * 0.5f, box.y + box.h - 2.0f));
}

void draw_goal_menu(Render *r, const GoalMenu *menu, const BuildGoal *current, const Vec2 *mouse) {
    if (!menu || !menu->visible) {
        return;
    }
    render_fill_rect(r, (Rect){0.0f, 0.0f, (f32)LOGICAL_W, (f32)LOGICAL_H}, (Color){6, 6, 14, 90}); // the game steps back
    const Vec2 c = menu->center;
    const i32 hover = mouse ? goal_menu_hit(menu, *mouse) : -1;
    const i32 now = current && current->active ? goal_menu_index_of((PadShape)current->shape, current->blue) : -1;

    fill_disc(r, c, GOAL_MENU_INNER, hover == GOAL_MENU_CENTER ? (Color){52, 58, 104, 245} : COL_BG);
    fill_disc(r, c, GOAL_MENU_INNER - 1.0f, hover == GOAL_MENU_CENTER ? (Color){52, 58, 104, 245} : COL_BG);
    ui_text_shadow(r, c.x, c.y - 4.0f, now >= 0 ? "AUCUN" : "OBJECTIF", hover == GOAL_MENU_CENTER ? COL_GOLD : COL_DIM, 1, ALIGN_CENTER);
    for (i32 i = 0; i < GOAL_MENU_ENTRIES; i++) {
        if (i != hover) {
            draw_entry(r, goal_menu_entry_box(menu, i), i, false, i == now);
        }
    }
    if (hover >= 0) { // drawn last: its border is not covered by the neighbours
        draw_entry(r, goal_menu_entry_box(menu, hover), hover, true, hover == now);
    }
    // What the highlighted entry (else the current goal) builds, under the middle disc.
    const i32 shown = hover >= 0 ? hover : now;
    if (shown >= 0) {
        PadShape shape;
        bool blue;
        goal_menu_entry(shown, &shape, &blue);
        const char *label = ui_product_label(pad_recipe(shape, blue));
        const f32 w = (f32)font_text_width(label, 1) + 10.0f;
        const Rect tip = {c.x - w * 0.5f, c.y + GOAL_MENU_INNER + 4.0f, w, 15.0f};
        ui_panel(r, tip, COL_GOLD);
        ui_text_shadow(r, c.x, tip.y + 4.0f, label, COL_TEXT, 1, ALIGN_CENTER);
    }
}
