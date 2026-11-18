#include "render/draw_debug.h"

#include "core/config.h"
#include "render/draw_ui.h"
#include "render/font.h"
#include "render/sprites.h"

static const Color COL_TITLE = {255, 224, 80, 255};
static const Color COL_GROUP = {140, 150, 190, 255};
static const Color COL_TEXT = {236, 236, 244, 255};
static const Color COL_HOVER_BG = {60, 66, 110, 255};
static const Color COL_BORDER = {120, 126, 160, 255};

// A small picture of what the entry spawns, `center` being its middle.
static void draw_icon(Render *r, DebugItem item, Vec2 center) {
    SpriteDraw d = {.palette = 0, .pos = center, .pivot_center = true};
    switch (item) {
    case DEBUG_CRATE_RED:
    case DEBUG_CRATE_BLUE:
        d.id = item == DEBUG_CRATE_BLUE ? SPR_CRATE_BLUE : SPR_CRATE_RED;
        d.scale_x = d.scale_y = 0.6f;
        break;
    case DEBUG_ALLY_NORMAL:
    case DEBUG_ALLY_STRONG:
    case DEBUG_ALLY_HEAVY:
    case DEBUG_ALLY_ARMORED:
        d.id = sprite_body(item - DEBUG_ALLY_NORMAL, POSE_IDLE0);
        d.scale_x = d.scale_y = 0.5f;
        break;
    case DEBUG_WEAPON_PISTOL:
        d.id = sprite_for_weapon(WEAPON_PISTOL);
        break;
    case DEBUG_WEAPON_UZI:
        d.id = sprite_for_weapon(WEAPON_UZI);
        break;
    case DEBUG_WEAPON_GRENADES:
    case DEBUG_WEAPON_STRONG_GRENADES:
        d.id = sprite_for_weapon(WEAPON_GRENADES);
        break;
    case DEBUG_WEAPON_ROCKET_LAUNCHER:
        d.id = sprite_for_weapon(WEAPON_ROCKET_LAUNCHER);
        d.scale_x = d.scale_y = 0.6f;
        break;
    case DEBUG_VEHICLE_TANK:
    case DEBUG_VEHICLE_HEAVY_TANK:
        d.id = item == DEBUG_VEHICLE_HEAVY_TANK ? SPR_HEAVY_TANK : SPR_TANK;
        d.scale_x = d.scale_y = 0.7f;
        break;
    default:
        return;
    }
    render_sprite(r, &d);
}

void draw_debug_menu(Render *r, const DebugMenu *menu, const Vec2 *mouse) {
    if (!menu || !menu->visible) {
        return;
    }
    const Rect panel = debug_menu_panel();
    ui_panel(r, panel, COL_BORDER);
    ui_text_shadow(r, panel.x + 6.0f, panel.y + 5.0f, "DEBUG : APPARITION", COL_TITLE, 1, ALIGN_LEFT);
    const i32 hover = mouse ? debug_menu_hit_test(menu, *mouse) : -1;
    for (i32 i = 0; i < DEBUG_ITEM_COUNT; i++) {
        const DebugItem item = (DebugItem)i;
        const Rect box = debug_menu_item_rect(item);
        if (debug_item_group(item)) {
            ui_text_shadow(r, box.x + 1.0f, box.y - 9.0f, debug_item_group(item), COL_GROUP, 1, ALIGN_LEFT);
        }
        if (i == hover) {
            render_fill_rect(r, box, COL_HOVER_BG);
            render_outline_rect(r, box, COL_TITLE);
        }
        draw_icon(r, item, vec2(box.x + 9.0f, box.y + box.h * 0.5f + 1.0f));
        ui_text_shadow(r, box.x + 20.0f, box.y + 2.0f, debug_item_label(item), i == hover ? COL_TITLE : COL_TEXT, 1, ALIGN_LEFT);
    }
}

void draw_toast(Render *r, const char *text) {
    if (!text || !text[0]) {
        return;
    }
    const f32 w = (f32)font_text_width(text, 1) + 16.0f;
    const Rect box = {((f32)LOGICAL_W - w) * 0.5f, 28.0f, w, 17.0f};
    ui_panel(r, box, COL_BORDER);
    ui_text_shadow(r, (f32)LOGICAL_W * 0.5f, box.y + 5.0f, text, COL_TEXT, 1, ALIGN_CENTER);
}
