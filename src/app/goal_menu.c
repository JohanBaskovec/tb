#include "app/goal_menu.h"

#include "core/config.h"

#include <math.h>

#define PI 3.14159265f

// Space needed around the centre so that the whole ring (entries included) stays on screen.
static f32 margin_x(void) {
    return GOAL_MENU_RADIUS + GOAL_MENU_ENTRY_W * 0.5f + 2.0f;
}

static f32 margin_y(void) {
    return GOAL_MENU_RADIUS + GOAL_MENU_ENTRY_H * 0.5f + 2.0f;
}

void goal_menu_open(GoalMenu *menu, Vec2 at) {
    menu->visible = true;
    menu->center = vec2(clampf(at.x, margin_x(), (f32)LOGICAL_W - margin_x()),
                        clampf(at.y, margin_y(), (f32)LOGICAL_H - margin_y()));
}

void goal_menu_close(GoalMenu *menu) {
    menu->visible = false;
}

void goal_menu_entry(i32 index, PadShape *shape, bool *blue) {
    *shape = (PadShape)(index % SHAPE_COUNT);
    *blue = index >= SHAPE_COUNT;
}

i32 goal_menu_index_of(PadShape shape, bool blue) {
    return (i32)shape + (blue ? SHAPE_COUNT : 0);
}

// Direction of an entry, in radians: the first one at the top, then clockwise.
static f32 entry_angle(i32 index) {
    return -PI * 0.5f + (f32)index * 2.0f * PI / (f32)GOAL_MENU_ENTRIES;
}

Rect goal_menu_entry_box(const GoalMenu *menu, i32 index) {
    const f32 a = entry_angle(index);
    const f32 x = menu->center.x + cosf(a) * GOAL_MENU_RADIUS, y = menu->center.y + sinf(a) * GOAL_MENU_RADIUS;
    return (Rect){roundf(x - GOAL_MENU_ENTRY_W * 0.5f), roundf(y - GOAL_MENU_ENTRY_H * 0.5f), GOAL_MENU_ENTRY_W,
                  GOAL_MENU_ENTRY_H};
}

i32 goal_menu_hit(const GoalMenu *menu, Vec2 p) {
    if (!menu->visible) {
        return -1;
    }
    const f32 dx = p.x - menu->center.x, dy = p.y - menu->center.y;
    const f32 r = sqrtf(dx * dx + dy * dy);
    if (r < GOAL_MENU_INNER) {
        return GOAL_MENU_CENTER;
    }
    if (r > GOAL_MENU_RADIUS + GOAL_MENU_ENTRY_H) {
        return -1;
    }
    // Sector of the angle, counted from the top (the first entry is centred on it) clockwise.
    f32 turns = (atan2f(dy, dx) + PI * 0.5f) / (2.0f * PI); // 0 at the top, going clockwise
    turns = turns - floorf(turns);
    return (i32)floorf(turns * (f32)GOAL_MENU_ENTRIES + 0.5f) % GOAL_MENU_ENTRIES;
}

u32 goal_shape_cells(PadShape shape, PadCoord out[PAD_CELLS]) {
    const i32 bottom = PAD_ROWS - 1;
    u32 n = 0;
    switch (shape) {
    case SHAPE_1X1:
        out[n++] = (PadCoord){0, (u8)bottom};
        break;
    case SHAPE_2X1_H:
        out[n++] = (PadCoord){0, (u8)bottom};
        out[n++] = (PadCoord){1, (u8)bottom};
        break;
    case SHAPE_2X1_V:
        out[n++] = (PadCoord){0, 0};
        out[n++] = (PadCoord){0, (u8)bottom};
        break;
    case SHAPE_L:
        out[n++] = (PadCoord){0, 0};
        out[n++] = (PadCoord){0, (u8)bottom};
        out[n++] = (PadCoord){1, (u8)bottom};
        break;
    case SHAPE_2X2:
        for (u8 r = 0; r < PAD_ROWS; r++) {
            for (u8 c = 0; c < 2; c++) {
                out[n++] = (PadCoord){c, r};
            }
        }
        break;
    case SHAPE_4X2:
        for (u8 r = 0; r < PAD_ROWS; r++) {
            for (u8 c = 0; c < PAD_COLS; c++) {
                out[n++] = (PadCoord){c, r};
            }
        }
        break;
    default:
        break;
    }
    return n;
}
