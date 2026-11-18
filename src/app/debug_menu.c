#include "app/debug_menu.h"

#include "core/config.h"
#include "game/crate.h"
#include "game/loot.h"
#include "game/pad.h"
#include "game/squad.h"
#include "game/tuning.h"

#include <math.h>

#define PANEL_W 148.0f
#define PANEL_TOP 26.0f
#define TITLE_H 15.0f
#define GROUP_H 11.0f
#define ITEM_H 13.0f
#define PAD 4.0f

typedef struct ItemDef {
    const char *label;
    const char *group; // set on the first entry of a group
} ItemDef;

static const ItemDef ITEMS[DEBUG_ITEM_COUNT] = {
    [DEBUG_CRATE_RED] = {"CAISSE ROUGE", "CAISSES"},
    [DEBUG_CRATE_BLUE] = {"CAISSE BLEUE", NULL},
    [DEBUG_ALLY_NORMAL] = {"SOLDAT", "ALLIÉS"},
    [DEBUG_ALLY_STRONG] = {"SOLDAT FORT", NULL},
    [DEBUG_ALLY_HEAVY] = {"SOLDAT LOURD", NULL},
    [DEBUG_ALLY_ARMORED] = {"LOURD BLINDÉ", NULL},
    [DEBUG_WEAPON_PISTOL] = {"PISTOLET", "ARMES"},
    [DEBUG_WEAPON_UZI] = {"UZI", NULL},
    [DEBUG_WEAPON_GRENADES] = {"GRENADES", NULL},
    [DEBUG_WEAPON_STRONG_GRENADES] = {"GRENADES PUISSANTES", NULL},
    [DEBUG_WEAPON_ROCKET_LAUNCHER] = {"LANCE-ROQUETTES", NULL},
    [DEBUG_VEHICLE_TANK] = {"TANK", "VÉHICULES"},
    [DEBUG_VEHICLE_HEAVY_TANK] = {"TANK LOURD", NULL},
};

const char *debug_item_label(DebugItem item) {
    return item >= 0 && item < DEBUG_ITEM_COUNT ? ITEMS[item].label : "";
}

const char *debug_item_group(DebugItem item) {
    return item >= 0 && item < DEBUG_ITEM_COUNT ? ITEMS[item].group : NULL;
}

// ---- Layout ------------------------------------------------------------------------------------

static f32 panel_x(void) {
    return (f32)LOGICAL_W - PANEL_W - 6.0f;
}

static f32 panel_height(void) {
    f32 h = TITLE_H + PAD * 2.0f;
    for (i32 i = 0; i < DEBUG_ITEM_COUNT; i++) {
        h += ITEMS[i].group ? GROUP_H : 0.0f;
        h += ITEM_H;
    }
    return h;
}

Rect debug_menu_panel(void) {
    return (Rect){panel_x(), PANEL_TOP, PANEL_W, panel_height()};
}

Rect debug_menu_item_rect(DebugItem item) {
    f32 y = PANEL_TOP + PAD + TITLE_H;
    for (i32 i = 0; i <= (i32)item && i < DEBUG_ITEM_COUNT; i++) {
        y += ITEMS[i].group ? GROUP_H : 0.0f;
        if (i == (i32)item) {
            return (Rect){panel_x() + PAD, y, PANEL_W - PAD * 2.0f, ITEM_H - 1.0f};
        }
        y += ITEM_H;
    }
    return (Rect){0.0f, 0.0f, 0.0f, 0.0f};
}

static bool contains(Rect r, Vec2 p) {
    return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

bool debug_menu_covers(const DebugMenu *menu, Vec2 logical) {
    return menu && menu->visible && contains(debug_menu_panel(), logical);
}

i32 debug_menu_hit_test(const DebugMenu *menu, Vec2 logical) {
    if (!menu || !menu->visible) {
        return -1;
    }
    for (i32 i = 0; i < DEBUG_ITEM_COUNT; i++) {
        if (contains(debug_menu_item_rect((DebugItem)i), logical)) {
            return i;
        }
    }
    return -1;
}

// ---- Spawning ----------------------------------------------------------------------------------

static bool rect_free(const Level *lv, f32 x, f32 y, f32 w, f32 h) {
    if (x < 0.0f || y < 0.0f || x + w > level_width_px(lv) || y + h > level_height_px(lv)) {
        return false;
    }
    const i32 tx0 = level_px_to_tile(x), tx1 = level_px_to_tile(x + w - 0.01f);
    const i32 ty0 = level_px_to_tile(y), ty1 = level_px_to_tile(y + h - 0.01f);
    for (i32 ty = ty0; ty <= ty1; ty++) {
        for (i32 tx = tx0; tx <= tx1; tx++) {
            if (level_is_solid(lv, tx, ty)) {
                return false;
            }
        }
    }
    return true;
}

// Is another crate on its way down through the columns `x .. x + w`? Two crates falling into the same cell would stay
// stuck on top of each other, so a new one goes elsewhere.
static bool crate_coming_down(const GameState *gs, f32 x, f32 w) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type != ENT_CRATE || e->pending_destroy || e->as.crate.state == CRATE_RESTING) {
            continue;
        }
        if (e->body.pos.x < x + w && x < e->body.pos.x + e->body.size.x) {
            return true;
        }
    }
    return false;
}

// Horizontal distances tried, in order, from the middle of the focus: its facing side first.
static const f32 SIDE_OFFSETS[] = {24.0f, -24.0f, 44.0f, -44.0f, 64.0f, -64.0f};

// A free spot for a rectangle `w` x `h` whose top is at `top`, beside the focus (never on the soldier itself: a crate
// would wait for it to move away, and a weapon would be picked up at once). `crate`: not in the column of a crate that
// is still coming down.
static bool find_spot(const GameState *gs, const DebugFocus *f, f32 w, f32 h, f32 top, bool crate, Vec2 *out) {
    const f32 face = f->facing < 0 ? -1.0f : 1.0f;
    for (usize i = 0; i < sizeof(SIDE_OFFSETS) / sizeof(SIDE_OFFSETS[0]); i++) {
        const f32 dx = SIDE_OFFSETS[i] * face;
        const f32 x = roundf(f->center.x + dx - w * 0.5f), y = roundf(MAX(top, 0.0f));
        if (rect_free(gs->level, x, y, w, h) && !(crate && crate_coming_down(gs, x, w))) {
            *out = vec2(x, y);
            return true;
        }
    }
    return false;
}

static bool spawn_weapon(GameState *gs, WeaponKind weapon, const DebugFocus *f) {
    Vec2 at;
    if (!find_spot(gs, f, TUNING_LOOT_W, TUNING_LOOT_H, f->top - 20.0f, false, &at)) {
        return false;
    }
    return entity_id_valid(loot_spawn_weapon(gs, weapon, weapon_def(weapon)->max_ammo, at));
}

bool debug_spawn(GameState *gs, DebugItem item, const DebugFocus *focus) {
    Vec2 at;
    switch (item) {
    case DEBUG_CRATE_RED:
    case DEBUG_CRATE_BLUE:
        if (!find_spot(gs, focus, TUNING_CRATE_SIZE, TUNING_CRATE_SIZE, focus->top - 40.0f, true, &at)) {
            return false;
        }
        return entity_id_valid(
            crate_spawn(gs, item == DEBUG_CRATE_BLUE ? CRATE_BLUE : CRATE_RED, at, CRATE_FALLING, -1));
    case DEBUG_ALLY_NORMAL:
    case DEBUG_ALLY_STRONG:
    case DEBUG_ALLY_HEAVY:
    case DEBUG_ALLY_ARMORED: {
        const SoldierVariant variant = (SoldierVariant)(item - DEBUG_ALLY_NORMAL);
        if (!find_spot(gs, focus, TUNING_COMPANION_W, TUNING_COMPANION_H, focus->bottom - TUNING_COMPANION_H, false, &at)) {
            return false;
        }
        return soldier_spawn(gs, focus->team, variant, vec2(at.x + TUNING_COMPANION_W * 0.5f, at.y + TUNING_COMPANION_H)) >= 0;
    }
    case DEBUG_WEAPON_PISTOL:
        return spawn_weapon(gs, WEAPON_PISTOL, focus);
    case DEBUG_WEAPON_UZI:
        return spawn_weapon(gs, WEAPON_UZI, focus);
    case DEBUG_WEAPON_GRENADES:
        return spawn_weapon(gs, WEAPON_GRENADES, focus);
    case DEBUG_WEAPON_STRONG_GRENADES:
        return spawn_weapon(gs, WEAPON_STRONG_GRENADES, focus);
    case DEBUG_WEAPON_ROCKET_LAUNCHER:
        return spawn_weapon(gs, WEAPON_ROCKET_LAUNCHER, focus);
    case DEBUG_VEHICLE_TANK:
    case DEBUG_VEHICLE_HEAVY_TANK:
        if (!find_spot(gs, focus, TUNING_LOOT_W, TUNING_LOOT_H, focus->top - 20.0f, false, &at)) {
            return false;
        }
        return entity_id_valid(product_spawn(gs, item == DEBUG_VEHICLE_HEAVY_TANK ? PRODUCT_HEAVY_TANK : PRODUCT_TANK,
                                             focus->team, at));
    default:
        return false;
    }
}
