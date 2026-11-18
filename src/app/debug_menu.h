// The debug spawn menu (F2): a list on the right of the screen where a click puts a crate, an ally, a weapon or a
// vehicle model next to the soldier being followed. A development tool, not part of the game: it changes the match
// (and its hash) only when used. No SDL.
//
// The menu is laid out in screen pixels (the logical 640x360 picture), not in the world like the pad menu.
#pragma once

#include "game/game.h"

typedef enum DebugItem {
    DEBUG_CRATE_RED,
    DEBUG_CRATE_BLUE,
    DEBUG_ALLY_NORMAL,
    DEBUG_ALLY_STRONG,
    DEBUG_ALLY_HEAVY,
    DEBUG_ALLY_ARMORED,
    DEBUG_WEAPON_PISTOL,
    DEBUG_WEAPON_UZI,
    DEBUG_WEAPON_GRENADES,
    DEBUG_WEAPON_STRONG_GRENADES,
    DEBUG_WEAPON_ROCKET_LAUNCHER,
    DEBUG_VEHICLE_TANK,
    DEBUG_VEHICLE_HEAVY_TANK,
    DEBUG_ITEM_COUNT
} DebugItem;

typedef struct DebugMenu {
    bool visible;
} DebugMenu;

const char *debug_item_label(DebugItem item);
// Group title shown above the first entry of a group ("CAISSES", "ALLIÉS"...), NULL for the other entries.
const char *debug_item_group(DebugItem item);

// Where things are, in logical screen pixels.
Rect debug_menu_panel(void);
Rect debug_menu_item_rect(DebugItem item);
// The entry under a logical position, or -1 (also -1 while the menu is hidden).
i32 debug_menu_hit_test(const DebugMenu *menu, Vec2 logical);
// Is the position over the menu (entry or not)? Then a click belongs to the menu, not to the weapon.
bool debug_menu_covers(const DebugMenu *menu, Vec2 logical);

// Who the things appear next to: a soldier (its team and the way it faces), or else a point of the screen.
typedef struct DebugFocus {
    Vec2 center;  // middle of the body
    f32 top, bottom; // body top and feet height
    i32 facing;   // -1 / +1
    i32 team;     // the allies' team
} DebugFocus;

// Puts the item next to the focus: crates, weapons and vehicles above it so that they fall to the ground, an ally
// beside it. Looks on the side it faces first, then the other, then further away. Returns false if there is no room
// or the entity pool is full.
bool debug_spawn(GameState *gs, DebugItem item, const DebugFocus *focus);
