#include "app/pad_menu.h"

#include "game/tuning.h"

static bool soldier_near_pad(const GameState *gs, i32 team) {
    const Rect pad = pad_rect(gs, team);
    const Vec2 c = body_center(&game_player_const(gs)->body);
    return c.x >= pad.x - TUNING_PAD_MENU_RANGE_X && c.x <= pad.x + pad.w + TUNING_PAD_MENU_RANGE_X &&
           c.y >= pad.y - TUNING_PAD_MENU_RANGE_Y && c.y <= pad.y + pad.h + TUNING_PAD_MENU_RANGE_Y;
}

// Entries are listed beside the pad (right, or left when the level ends there), growing upward from
// the pad's bottom edge; a long list wraps into a second column.
static void layout(PadMenu *m, const GameState *gs) {
    const Rect pad = pad_rect(gs, m->team);
    const f32 col_w = PAD_MENU_ENTRY_W + PAD_MENU_GAP;
    const u32 columns = (m->options.count + PAD_MENU_MAX_ROWS - 1) / PAD_MENU_MAX_ROWS;
    f32 x0 = pad.x + pad.w + PAD_MENU_GAP * 2.0f;
    if (x0 + (f32)columns * col_w > level_width_px(gs->level)) {
        x0 = pad.x - PAD_MENU_GAP * 2.0f - (f32)columns * col_w + PAD_MENU_GAP;
    }
    const f32 bottom = pad.y + pad.h;
    for (u32 i = 0; i < m->options.count; i++) {
        const u32 col = i / PAD_MENU_MAX_ROWS, row = i % PAD_MENU_MAX_ROWS;
        const u32 rows_in_col = MIN(m->options.count - col * PAD_MENU_MAX_ROWS, (u32)PAD_MENU_MAX_ROWS);
        const f32 y = bottom - (f32)(rows_in_col - row) * (PAD_MENU_ENTRY_H + PAD_MENU_GAP) + PAD_MENU_GAP;
        m->rects[i] = (Rect){x0 + (f32)col * col_w, MAX(0.0f, y), PAD_MENU_ENTRY_W, PAD_MENU_ENTRY_H};
    }
}

void pad_menu_refresh(PadMenu *m, const GameState *gs, i32 team) {
    m->team = team;
    m->visible = false;
    m->options.count = 0;
    if (!pad_usable(gs, team) || !soldier_near_pad(gs, team)) {
        m->has_selection = false;
        return;
    }
    pad_read_grid(gs, team, &m->grid, NULL);
    pad_options(&m->grid, &m->options);
    if (m->options.count == 0) {
        m->has_selection = false;
        return;
    }
    m->visible = true;
    m->selected = 0;
    for (u32 i = 0; i < m->options.count && m->has_selection; i++) {
        if (m->options.items[i].shape == m->selected_shape && m->options.items[i].blue == m->selected_blue) {
            m->selected = (i32)i;
        }
    }
    m->selected_shape = m->options.items[m->selected].shape;
    m->selected_blue = m->options.items[m->selected].blue;
    m->has_selection = true;
    layout(m, gs);
}

static bool build_entry(PadMenu *m, GameState *gs, i32 index) {
    if (!m->visible || index < 0 || index >= (i32)m->options.count) {
        return false;
    }
    const PadOption *o = &m->options.items[index];
    const bool built = pad_build(gs, m->team, o->shape, o->blue);
    pad_menu_refresh(m, gs, m->team);
    return built;
}

bool pad_menu_handle_input(PadMenu *m, GameState *gs, const InputFrame *in) {
    if (!m->visible) {
        return false;
    }
    if (input_pressed(in, ACT_SWITCH)) {
        m->selected = (m->selected + 1) % (i32)m->options.count;
        m->selected_shape = m->options.items[m->selected].shape;
        m->selected_blue = m->options.items[m->selected].blue;
    }
    if (input_pressed(in, ACT_CONFIRM)) {
        return build_entry(m, gs, m->selected);
    }
    return false;
}

i32 pad_menu_hit_test(const PadMenu *m, Vec2 world) {
    if (!m->visible) {
        return -1;
    }
    for (u32 i = 0; i < m->options.count; i++) {
        const Rect r = m->rects[i];
        if (world.x >= r.x && world.x < r.x + r.w && world.y >= r.y && world.y < r.y + r.h) {
            return (i32)i;
        }
    }
    return -1;
}

bool pad_menu_click(PadMenu *m, GameState *gs, Vec2 world) {
    return build_entry(m, gs, pad_menu_hit_test(m, world));
}
