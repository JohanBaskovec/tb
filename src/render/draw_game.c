#include "render/draw_game.h"

#include "core/config.h"
#include "game/base.h"
#include "game/combat.h"
#include "game/tuning.h"
#include "render/draw_ui.h"

#include <stdio.h>

#include <math.h>

// Placeholder palette (final art comes later).
static const Color COL_LADDER = {156, 116, 64, 255};
static const Color COL_WATER = {36, 96, 190, 255};
static const Color COL_WATER_TOP = {110, 170, 240, 255};
static const Color COL_EYE = {255, 255, 255, 255};
static const Color COL_CRATE_RED = {208, 64, 56, 255};
static const Color COL_CRATE_BLUE = {56, 112, 226, 255};
static const Color COL_WOOD = {142, 98, 52, 255};
static const Color COL_WOOD_DARK = {92, 60, 30, 255};
static const Color COL_CHUTE = {240, 236, 220, 255};
static const Color COL_CHUTE_STRIPE = {214, 190, 120, 255};

static const Color COL_PAD_PLATE = {70, 190, 170, 255};
static const Color COL_PAD_CELL = {56, 120, 118, 255};
static const Color COL_MENU_BG = {20, 22, 36, 235};
static const Color COL_MENU_BORDER = {120, 126, 160, 255};
static const Color COL_MENU_SELECTED = {255, 224, 80, 255};

// One placeholder color per product (final art comes later).
static const Color PRODUCT_COLORS[PRODUCT_COUNT] = {
    [PRODUCT_PISTOL] = {190, 190, 200, 255},
    [PRODUCT_UZI] = {120, 120, 140, 255},
    [PRODUCT_GRENADES] = {110, 170, 70, 255},
    [PRODUCT_STRONG_GRENADES] = {190, 230, 60, 255},
    [PRODUCT_COMPANION] = {240, 170, 120, 255},
    [PRODUCT_STRONG_COMPANION] = {240, 120, 70, 255},
    [PRODUCT_ROCKET_LAUNCHER] = {230, 90, 40, 255},
    [PRODUCT_HEAVY_COMPANION] = {180, 90, 200, 255},
    [PRODUCT_HEAVY_ARMORED_COMPANION] = {120, 60, 160, 255},
    [PRODUCT_TANK] = {90, 140, 90, 255},
    [PRODUCT_HEAVY_TANK] = {50, 90, 60, 255},
};

static const Color WEAPON_COLORS[WEAPON_COUNT] = {
    [WEAPON_FISTS] = {240, 200, 170, 255},
    [WEAPON_PISTOL] = {190, 190, 200, 255},
    [WEAPON_UZI] = {120, 120, 140, 255},
    [WEAPON_GRENADES] = {110, 170, 70, 255},
    [WEAPON_STRONG_GRENADES] = {190, 230, 60, 255},
    [WEAPON_ROCKET_LAUNCHER] = {230, 90, 40, 255},
};
static const Color COL_HP_BACK = {40, 20, 24, 255};
static const Color COL_HP = {90, 220, 100, 255};
static const Color COL_HP_LOW = {230, 70, 60, 255};
static const Color COL_FLASH = {255, 255, 255, 255};
static const Color COL_DUMMY = {196, 160, 104, 255};
static const Color COL_DUMMY_DARK = {132, 104, 64, 255};
static const Color COL_BULLET = {255, 236, 120, 255};

static Rect rect_at(i32 ox, i32 oy, f32 x, f32 y, f32 w, f32 h) {
    return (Rect){x - (f32)ox, y - (f32)oy, w, h};
}

static void fill_circle(Render *r, Vec2 c, f32 radius, Color color);

static u32 tile_hash(i32 tx, i32 ty) {
    u32 h = (u32)tx * 73856093u ^ (u32)ty * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

static Color shade(Color c, f32 k) {
    return (Color){(u8)clampf((f32)c.r * k, 0.0f, 255.0f), (u8)clampf((f32)c.g * k, 0.0f, 255.0f),
                   (u8)clampf((f32)c.b * k, 0.0f, 255.0f), c.a};
}

// A ridge line made of a few sine waves: `scroll` is how fast it follows the camera (parallax).
static void draw_ridge(Render *r, i32 ox, i32 oy, f32 scroll, f32 base, f32 amp, f32 freq, f32 phase, Color color) {
    const f32 base_y = base - (f32)oy * scroll * 0.5f;
    for (i32 sx = 0; sx < LOGICAL_W; sx += 2) {
        const f32 wx = (f32)sx + (f32)ox * scroll;
        const f32 hgt = amp * (0.6f * sinf(wx * freq + phase) + 0.3f * sinf(wx * freq * 2.7f + phase * 1.7f) +
                               0.1f * sinf(wx * freq * 6.1f));
        const f32 top = roundf(base_y - hgt);
        if (top < (f32)LOGICAL_H) {
            render_fill_rect(r, (Rect){(f32)sx, top, 2.0f, (f32)LOGICAL_H - top}, color);
        }
    }
}

static void draw_cloud(Render *r, f32 x, f32 y, f32 size, Color c) {
    fill_circle(r, vec2(x, y), size, c);
    fill_circle(r, vec2(x + size * 1.1f, y + size * 0.25f), size * 0.8f, c);
    fill_circle(r, vec2(x - size * 1.0f, y + size * 0.3f), size * 0.7f, c);
    render_fill_rect(r, (Rect){x - size * 1.6f, y + size * 0.3f, size * 3.4f, size * 0.7f}, c);
}

// Dusk sky, a low sun, clouds and two rows of hills that scroll slower than the level.
static void draw_background(Render *r, i32 ox, i32 oy) {
    const int bands = 12;
    const f32 band_h = (f32)LOGICAL_H / (f32)bands;
    for (int i = 0; i < bands; i++) {
        const f32 t = (f32)i / (f32)(bands - 1);
        const Color c = {(u8)lerpf(34.0f, 214.0f, t * t), (u8)lerpf(40.0f, 128.0f, t * t), (u8)lerpf(86.0f, 110.0f, t), 255};
        render_fill_rect(r, (Rect){0.0f, (f32)i * band_h, (f32)LOGICAL_W, band_h + 1.0f}, c);
    }
    const f32 sun_x = 470.0f - (f32)ox * 0.02f, sun_y = 236.0f - (f32)oy * 0.05f;
    fill_circle(r, vec2(sun_x, sun_y), 30.0f, (Color){255, 196, 120, 255});
    fill_circle(r, vec2(sun_x, sun_y), 24.0f, (Color){255, 226, 160, 255});
    const Color cloud = {236, 180, 170, 200};
    for (int i = 0; i < 6; i++) {
        const f32 period = (f32)LOGICAL_W + 200.0f;
        f32 x = fmodf((f32)(i * 263) - (f32)ox * 0.1f, period);
        if (x < 0.0f) {
            x += period;
        }
        draw_cloud(r, x - 100.0f, 50.0f + (f32)((i * 37) % 90) - (f32)oy * 0.05f, 9.0f + (f32)(i % 3) * 3.0f, cloud);
    }
    draw_ridge(r, ox, oy, 0.15f, 290.0f, 60.0f, 0.009f, 0.7f, (Color){112, 74, 110, 255});
    draw_ridge(r, ox, oy, 0.35f, 330.0f, 45.0f, 0.014f, 2.1f, (Color){74, 50, 84, 255});
}

static const Color COL_GRASS = {108, 178, 84, 255};
static const Color COL_GRASS_DARK = {62, 120, 60, 255};
static const Color COL_DIRT = {118, 84, 74, 255};
static const Color COL_PEBBLE = {148, 112, 96, 255};
static const Color COL_PLANK = {186, 140, 80, 255};
static const Color COL_PLANK_DARK = {120, 84, 44, 255};

static void draw_one_way_top(Render *r, f32 x, f32 y) {
    const f32 t = (f32)TILE_SIZE;
    render_fill_rect(r, (Rect){x, y, t, 5.0f}, COL_PLANK);
    render_fill_rect(r, (Rect){x, y, t, 1.0f}, shade(COL_PLANK, 1.2f));
    render_fill_rect(r, (Rect){x, y + 5.0f, t, 1.0f}, COL_PLANK_DARK);
    render_fill_rect(r, (Rect){x + 7.0f, y + 1.0f, 1.0f, 4.0f}, COL_PLANK_DARK); // plank joint
    render_fill_rect(r, (Rect){x + 2.0f, y + 2.0f, 1.0f, 1.0f}, COL_PLANK_DARK);  // nails
    render_fill_rect(r, (Rect){x + 12.0f, y + 2.0f, 1.0f, 1.0f}, COL_PLANK_DARK);
}

// Earth: grass on the top layer, then dirt that darkens with depth, with a few pebbles.
static void draw_solid(Render *r, const Level *lv, i32 tx, i32 ty, f32 x, f32 y) {
    const f32 t = (f32)TILE_SIZE;
    i32 depth = 0;
    while (depth < 4 && level_tile(lv, tx, ty - depth - 1) == TILE_SOLID) {
        depth++;
    }
    const Color dirt = shade(COL_DIRT, 1.0f - 0.12f * (f32)depth);
    render_fill_rect(r, (Rect){x, y, t, t}, dirt);
    const u32 h = tile_hash(tx, ty);
    for (int k = 0; k < 2; k++) {
        const u32 v = h >> (k * 8);
        render_fill_rect(r, (Rect){x + (f32)(v % 13) + 1.0f, y + (f32)((v >> 4) % 11) + 3.0f, 2.0f, 1.0f},
                         shade(COL_PEBBLE, 1.0f - 0.12f * (f32)depth));
    }
    if (level_tile(lv, tx - 1, ty) != TILE_SOLID) {
        render_fill_rect(r, (Rect){x, y, 1.0f, t}, shade(dirt, 1.25f));
    }
    if (level_tile(lv, tx + 1, ty) != TILE_SOLID) {
        render_fill_rect(r, (Rect){x + t - 1.0f, y, 1.0f, t}, shade(dirt, 0.75f));
    }
    if (depth == 0 && level_tile(lv, tx, ty - 1) != TILE_HAZARD) {
        render_fill_rect(r, (Rect){x, y, t, 4.0f}, COL_GRASS);
        render_fill_rect(r, (Rect){x, y + 4.0f, t, 1.0f}, COL_GRASS_DARK);
        render_fill_rect(r, (Rect){x + (f32)(h % 5), y + 5.0f, 2.0f, 1.0f}, COL_GRASS_DARK); // ragged edge
        render_fill_rect(r, (Rect){x + 8.0f + (f32)((h >> 3) % 6), y + 5.0f, 3.0f, 1.0f}, COL_GRASS_DARK);
        if (level_tile(lv, tx, ty - 1) == TILE_EMPTY) { // blades sticking out
            render_fill_rect(r, (Rect){x + (f32)((h >> 6) % 14), y - 2.0f, 1.0f, 2.0f}, COL_GRASS);
            render_fill_rect(r, (Rect){x + (f32)((h >> 10) % 14) + 1.0f, y - 1.0f, 1.0f, 1.0f}, COL_GRASS);
        }
    }
}

static void draw_water(Render *r, const Level *lv, i32 tx, i32 ty, f32 x, f32 y, u64 tick) {
    const f32 t = (f32)TILE_SIZE;
    render_fill_rect(r, (Rect){x, y, t, t}, COL_WATER);
    if (level_tile(lv, tx, ty - 1) != TILE_HAZARD) { // the surface ripples
        const f32 phase = (f32)tick * 0.08f + (f32)tx * 1.3f;
        render_fill_rect(r, (Rect){x, y, t, 2.0f}, COL_WATER_TOP);
        render_fill_rect(r, (Rect){x + 4.0f + roundf(sinf(phase) * 3.0f), y + 4.0f, 5.0f, 1.0f}, COL_WATER_TOP);
    } else if ((tile_hash(tx, ty) + (u32)(tick / 20)) % 7 == 0) { // a bubble now and then
        render_fill_rect(r, (Rect){x + 6.0f, y + 6.0f, 2.0f, 2.0f}, COL_WATER_TOP);
    }
}

static void draw_tile(Render *r, const Level *lv, i32 tx, i32 ty, f32 x, f32 y, u64 tick) {
    const f32 t = (f32)TILE_SIZE;
    switch (level_tile(lv, tx, ty)) {
    case TILE_SOLID:
        draw_solid(r, lv, tx, ty, x, y);
        break;
    case TILE_PLATFORM:
        draw_one_way_top(r, x, y);
        if (level_tile(lv, tx, ty + 1) == TILE_EMPTY && (level_tile(lv, tx - 1, ty) != TILE_PLATFORM ||
                                                         level_tile(lv, tx + 1, ty) != TILE_PLATFORM)) {
            render_fill_rect(r, (Rect){x + 6.0f, y + 6.0f, 3.0f, 4.0f}, COL_PLANK_DARK); // end bracket
        }
        break;
    case TILE_LADDER:
        render_fill_rect(r, (Rect){x + 3.0f, y, 2.0f, t}, COL_LADDER);
        render_fill_rect(r, (Rect){x + 11.0f, y, 2.0f, t}, COL_LADDER);
        for (int i = 0; i < 4; i++) {
            render_fill_rect(r, (Rect){x + 5.0f, y + 2.0f + (f32)(i * 4), 6.0f, 1.0f}, COL_LADDER);
        }
        if (level_is_one_way(lv, tx, ty)) {
            draw_one_way_top(r, x, y);
        }
        break;
    case TILE_HAZARD:
        draw_water(r, lv, tx, ty, x, y, tick);
        break;
    default:
        break;
    }
}

static void draw_tiles(Render *r, const Level *lv, i32 ox, i32 oy, u64 tick) {
    i32 tx0 = MAX(0, ox / TILE_SIZE);
    i32 ty0 = MAX(0, oy / TILE_SIZE);
    i32 tx1 = MIN(lv->width - 1, (ox + LOGICAL_W) / TILE_SIZE);
    i32 ty1 = MIN(lv->height - 1, (oy + LOGICAL_H) / TILE_SIZE);
    for (i32 ty = ty0; ty <= ty1; ty++) {
        for (i32 tx = tx0; tx <= tx1; tx++) {
            draw_tile(r, lv, tx, ty, (f32)(tx * TILE_SIZE - ox), (f32)(ty * TILE_SIZE - oy), tick);
        }
    }
}

static void fill_circle(Render *r, Vec2 c, f32 radius, Color color) {
    for (f32 dy = -radius; dy <= radius; dy += 1.0f) {
        const f32 half = sqrtf(MAX(radius * radius - dy * dy, 0.0f));
        render_fill_rect(r, (Rect){c.x - half, c.y + dy, half * 2.0f, 1.0f}, color);
    }
}

static void draw_hp_bar(Render *r, Rect box, f32 fraction) {
    render_fill_rect(r, box, COL_HP_BACK);
    render_fill_rect(r, (Rect){box.x, box.y, box.w * clampf(fraction, 0.0f, 1.0f), box.h},
                     fraction > 0.3f ? COL_HP : COL_HP_LOW);
}

static const Color COL_STEEL = {150, 156, 170, 255};
static const Color COL_MARKER = {255, 224, 80, 255};

static void draw_soldier(Render *r, const Soldier *s, bool controlled, f32 alpha, i32 ox, i32 oy) {
    const Companion *c = &s->c;
    const f32 x = roundf(lerpf(c->prev_pos.x, c->body.pos.x, alpha));
    const f32 y = roundf(lerpf(c->prev_pos.y, c->body.pos.y, alpha));
    const f32 w = c->body.size.x, h = c->body.size.y;
    const bool flash = c->hurt_ticks > 0 && (c->hurt_ticks / 2) % 2 == 0;
    const Color body = flash ? COL_FLASH : ui_team_color(s->team);
    const Color dark = flash ? COL_FLASH : ui_team_color_dark(s->team);
    render_fill_rect(r, rect_at(ox, oy, x, y, w, h), body);
    render_fill_rect(r, rect_at(ox, oy, x, y + h - 4.0f, w, 4.0f), dark);
    // Variants: a stripe for the strong one, shoulder pads for the heavy ones, plating for the armored one.
    if (s->variant == SOLDIER_STRONG) {
        render_fill_rect(r, rect_at(ox, oy, x, y + 10.0f, w, 2.0f), COL_EYE);
    } else if (s->variant == SOLDIER_HEAVY || s->variant == SOLDIER_HEAVY_ARMORED) {
        render_fill_rect(r, rect_at(ox, oy, x - 2.0f, y + 6.0f, 2.0f, 6.0f), dark);
        render_fill_rect(r, rect_at(ox, oy, x + w, y + 6.0f, 2.0f, 6.0f), dark);
        if (s->variant == SOLDIER_HEAVY_ARMORED) {
            render_fill_rect(r, rect_at(ox, oy, x + 1.0f, y + 8.0f, w - 2.0f, 8.0f), COL_STEEL);
            render_fill_rect(r, rect_at(ox, oy, x + 1.0f, y + 12.0f, w - 2.0f, 1.0f), dark);
        }
    }
    const f32 eye_x = c->facing > 0 ? x + w - 5.0f : x + 3.0f;
    render_fill_rect(r, rect_at(ox, oy, eye_x, y + 4.0f, 2.0f, 3.0f), COL_EYE);
    // The weapon in hand (a colored barrel), or the fist while punching.
    const f32 hand_y = y + TUNING_MUZZLE_HEIGHT - 1.0f;
    if (c->weapon != WEAPON_FISTS) {
        // A two-pixel barrel from the shoulder along the aim direction.
        const Vec2 from = vec2(x + w * 0.5f - (f32)ox, y + TUNING_MUZZLE_HEIGHT - (f32)oy);
        const Vec2 to = vec2_add(from, vec2_scale(c->aim_dir, 10.0f));
        const Vec2 side = vec2_scale(vec2(-c->aim_dir.y, c->aim_dir.x), 1.0f);
        render_line(r, from, to, WEAPON_COLORS[c->weapon]);
        render_line(r, vec2_add(from, side), vec2_add(to, side), WEAPON_COLORS[c->weapon]);
    } else if (c->punch_ticks > 0) {
        const f32 fx = c->facing > 0 ? x + w : x - 6.0f;
        render_fill_rect(r, rect_at(ox, oy, fx, hand_y - 1.0f, 6.0f, 5.0f), WEAPON_COLORS[WEAPON_FISTS]);
    }
    if (controlled) { // a small arrow above the head
        const f32 mx = x + w * 0.5f - (f32)ox, my = y - 9.0f - (f32)oy;
        render_line(r, vec2(mx - 3.0f, my), vec2(mx + 3.0f, my), COL_MARKER);
        render_line(r, vec2(mx - 2.0f, my + 1.0f), vec2(mx + 2.0f, my + 1.0f), COL_MARKER);
        render_line(r, vec2(mx - 1.0f, my + 2.0f), vec2(mx + 1.0f, my + 2.0f), COL_MARKER);
        render_line(r, vec2(mx, my + 3.0f), vec2(mx, my + 3.0f), COL_MARKER);
    } else if (c->hp < c->max_hp) {
        draw_hp_bar(r, rect_at(ox, oy, x, y - 5.0f, w, 3.0f), (f32)c->hp / (f32)c->max_hp);
    }
}

static void draw_soldiers(Render *r, const GameState *gs, f32 alpha, i32 ox, i32 oy) {
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        if (gs->soldiers[i].alive) {
            draw_soldier(r, &gs->soldiers[i], i == gs->controlled, alpha, ox, oy);
        }
    }
}


// A wooden frame with crossed planks; the colored panel tells red from blue (and from team colors).
static void draw_crate_sprite(Render *r, f32 x, f32 y, CrateKind kind) {
    const f32 s = TUNING_CRATE_SIZE;
    render_fill_rect(r, (Rect){x, y, s, s}, COL_WOOD);
    render_fill_rect(r, (Rect){x + 2.0f, y + 2.0f, s - 4.0f, s - 4.0f},
                     kind == CRATE_BLUE ? COL_CRATE_BLUE : COL_CRATE_RED);
    render_line(r, vec2(x + 2.0f, y + 2.0f), vec2(x + s - 3.0f, y + s - 3.0f), COL_WOOD_DARK);
    render_line(r, vec2(x + s - 3.0f, y + 2.0f), vec2(x + 2.0f, y + s - 3.0f), COL_WOOD_DARK);
}

static void draw_parachute(Render *r, f32 x, f32 y) {
    const f32 s = TUNING_CRATE_SIZE;
    render_fill_rect(r, (Rect){x - 2.0f, y - 12.0f, s + 4.0f, 5.0f}, COL_CHUTE);
    render_fill_rect(r, (Rect){x + 2.0f, y - 12.0f, 3.0f, 5.0f}, COL_CHUTE_STRIPE);
    render_fill_rect(r, (Rect){x + s - 5.0f, y - 12.0f, 3.0f, 5.0f}, COL_CHUTE_STRIPE);
    render_line(r, vec2(x - 1.0f, y - 7.0f), vec2(x + 2.0f, y), COL_CHUTE);
    render_line(r, vec2(x + s, y - 7.0f), vec2(x + s - 3.0f, y), COL_CHUTE);
}

static void draw_crates(Render *r, const GameState *gs, f32 alpha, i32 ox, i32 oy, u32 flags) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type != ENT_CRATE) {
            continue;
        }
        const f32 x = roundf(lerpf(e->prev_pos.x, e->body.pos.x, alpha)) - (f32)ox;
        const f32 y = roundf(lerpf(e->prev_pos.y, e->body.pos.y, alpha)) - (f32)oy;
        if (x < -24.0f || x > (f32)LOGICAL_W + 8.0f || y < -24.0f || y > (f32)LOGICAL_H + 8.0f) {
            continue;
        }
        const CrateState st = e->as.crate.state;
        draw_crate_sprite(r, x, y, e->as.crate.kind);
        if (st == CRATE_DESCENDING) {
            draw_parachute(r, x, y);
        }
        if (flags & DRAW_DEBUG) { // outline by state: descending cyan, falling orange, carried magenta
            const Color outline = st == CRATE_DESCENDING ? (Color){80, 230, 255, 255}
                                  : st == CRATE_FALLING  ? (Color){255, 150, 40, 255}
                                  : st == CRATE_CARRIED  ? (Color){230, 80, 220, 255}
                                                         : (Color){60, 60, 80, 255};
            render_outline_rect(r, (Rect){x, y, TUNING_CRATE_SIZE, TUNING_CRATE_SIZE}, outline);
        }
    }
}

static const Color COL_STONE = {96, 92, 110, 255};
static const Color COL_STONE_DARK = {60, 56, 72, 255};
static const Color COL_WINDOW = {255, 214, 110, 255};
static const Color COL_POLE = {200, 200, 210, 255};

// A bunker in the team colors with a flag on top; cracked when hurt, rubble once destroyed.
static void draw_base(Render *r, const GameState *gs, i32 team, i32 ox, i32 oy) {
    const Team *t = &gs->teams[team];
    const Rect b = base_rect(gs, team);
    const f32 x = b.x - (f32)ox, y = b.y - (f32)oy, w = b.w, h = b.h;
    if (x + w < 0.0f || x > (f32)LOGICAL_W || y + h + 8.0f < 0.0f || y - 16.0f > (f32)LOGICAL_H) {
        return;
    }
    const Color body = ui_team_color(team), dark = ui_team_color_dark(team);
    if (t->base_hp <= 0) { // ruin: a heap of blocks and a broken pole
        render_fill_rect(r, (Rect){x + 2.0f, y + h - 8.0f, w - 4.0f, 8.0f}, COL_STONE_DARK);
        render_fill_rect(r, (Rect){x + 5.0f, y + h - 13.0f, 10.0f, 6.0f}, dark);
        render_fill_rect(r, (Rect){x + 18.0f, y + h - 11.0f, 8.0f, 4.0f}, COL_STONE);
        render_fill_rect(r, (Rect){x + 27.0f, y + h - 15.0f, 7.0f, 8.0f}, dark);
        render_line(r, vec2(x + 9.0f, y + h - 13.0f), vec2(x + 4.0f, y + h - 24.0f), COL_POLE);
        return;
    }
    const bool flash = t->base_hurt_ticks > SECONDS_TO_TICKS(TUNING_BASE_ALERT_S) - 6;
    render_fill_rect(r, (Rect){x, y + h - 4.0f, w, 4.0f}, COL_STONE_DARK);              // foundation
    render_fill_rect(r, (Rect){x + 3.0f, y + 14.0f, w - 6.0f, h - 18.0f}, flash ? COL_FLASH : dark); // walls
    render_fill_rect(r, (Rect){x + 3.0f, y + 14.0f, w - 6.0f, 3.0f}, flash ? COL_FLASH : body);
    render_fill_rect(r, (Rect){x, y + 9.0f, w, 5.0f}, COL_STONE);                       // roof
    render_fill_rect(r, (Rect){x + 4.0f, y + 6.0f, w - 8.0f, 3.0f}, COL_STONE);
    render_fill_rect(r, (Rect){x + w * 0.5f - 5.0f, y + h - 16.0f, 10.0f, 12.0f}, COL_STONE_DARK); // door
    render_fill_rect(r, (Rect){x + 7.0f, y + 20.0f, 5.0f, 4.0f}, COL_WINDOW);
    render_fill_rect(r, (Rect){x + w - 12.0f, y + 20.0f, 5.0f, 4.0f}, COL_WINDOW);
    render_fill_rect(r, (Rect){x + w - 9.0f, y - 10.0f, 1.0f, 16.0f}, COL_POLE);         // flag
    render_fill_rect(r, (Rect){x + w - 8.0f, y - 10.0f, 9.0f, 6.0f}, body);
    if (t->base_hp * 2 < TUNING_BASE_HP) { // cracks
        render_line(r, vec2(x + 6.0f, y + 15.0f), vec2(x + 11.0f, y + 26.0f), COL_STONE_DARK);
        render_line(r, vec2(x + 11.0f, y + 26.0f), vec2(x + 9.0f, y + 33.0f), COL_STONE_DARK);
        render_line(r, vec2(x + w - 5.0f, y + 16.0f), vec2(x + w - 13.0f, y + 30.0f), COL_STONE_DARK);
    }
    if (t->base_hp * 4 < TUNING_BASE_HP) {
        render_fill_rect(r, (Rect){x + 4.0f, y + 6.0f, 9.0f, 4.0f}, (Color){24, 22, 30, 255}); // a hole in the roof
    }
}

static void draw_bases(Render *r, const GameState *gs, i32 ox, i32 oy) {
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        if (gs->teams[t].has_base) {
            draw_base(r, gs, t, ox, oy);
        }
    }
}

// The 4x2 area of every pad: a plate under it and faint cell outlines.
static void draw_pads(Render *r, const GameState *gs, i32 ox, i32 oy) {
    for (i32 team = 0; team < MAX_TEAMS; team++) {
        if (!gs->level->pads[team].present) {
            continue;
        }
        const Rect pr = pad_rect(gs, team);
        render_fill_rect(r, rect_at(ox, oy, pr.x - 2.0f, pr.y + pr.h, pr.w + 4.0f, 3.0f), ui_team_color(team));
        render_fill_rect(r, rect_at(ox, oy, pr.x + 2.0f, pr.y + pr.h, pr.w - 4.0f, 1.0f), COL_PAD_PLATE);
        for (i32 row = 0; row < PAD_ROWS; row++) {
            for (i32 col = 0; col < PAD_COLS; col++) {
                render_outline_rect(r, rect_at(ox, oy, pr.x + (f32)(col * TILE_SIZE), pr.y + (f32)(row * TILE_SIZE),
                                               (f32)TILE_SIZE, (f32)TILE_SIZE), COL_PAD_CELL);
            }
        }
    }
}

static void draw_products(Render *r, const GameState *gs, f32 alpha, i32 ox, i32 oy) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type != ENT_PRODUCT) {
            continue;
        }
        const f32 x = roundf(lerpf(e->prev_pos.x, e->body.pos.x, alpha));
        const f32 y = roundf(lerpf(e->prev_pos.y, e->body.pos.y, alpha));
        const Color c = PRODUCT_COLORS[e->as.product.kind];
        render_fill_rect(r, rect_at(ox, oy, x, y, e->body.size.x, e->body.size.y), COL_WOOD_DARK);
        render_fill_rect(r, rect_at(ox, oy, x + 1.0f, y + 1.0f, e->body.size.x - 2.0f, e->body.size.y - 2.0f), c);
    }
}

static void draw_loot_weapons(Render *r, const GameState *gs, f32 alpha, i32 ox, i32 oy) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type != ENT_WEAPON) {
            continue;
        }
        const f32 x = roundf(lerpf(e->prev_pos.x, e->body.pos.x, alpha));
        const f32 y = roundf(lerpf(e->prev_pos.y, e->body.pos.y, alpha));
        const Color c = WEAPON_COLORS[e->as.weapon.kind];
        const f32 w = e->body.size.x, h = e->body.size.y;
        render_fill_rect(r, rect_at(ox, oy, x, y + h - 7.0f, w, 7.0f), COL_WOOD_DARK); // a small case
        render_fill_rect(r, rect_at(ox, oy, x + 1.0f, y + h - 6.0f, w - 2.0f, 5.0f), c);
        render_fill_rect(r, rect_at(ox, oy, x + 2.0f, y + h - 9.0f, w - 4.0f, 2.0f), c); // the weapon's top
    }
}

static void draw_dummies(Render *r, const GameState *gs, f32 alpha, i32 ox, i32 oy) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type != ENT_DUMMY) {
            continue;
        }
        const Dummy *d = &e->as.dummy;
        const f32 x = roundf(lerpf(e->prev_pos.x, e->body.pos.x, alpha));
        const f32 y = roundf(lerpf(e->prev_pos.y, e->body.pos.y, alpha));
        const f32 w = e->body.size.x, h = e->body.size.y;
        const bool flash = d->hurt_ticks > 0 && (d->hurt_ticks / 2) % 2 == 0;
        render_fill_rect(r, rect_at(ox, oy, x + w * 0.5f - 1.0f, y + h - 4.0f, 2.0f, 4.0f), COL_WOOD_DARK); // post
        render_fill_rect(r, rect_at(ox, oy, x, y + 6.0f, w, h - 12.0f), flash ? COL_FLASH : COL_DUMMY);
        render_fill_rect(r, rect_at(ox, oy, x + 3.0f, y, w - 6.0f, 7.0f), flash ? COL_FLASH : COL_DUMMY_DARK);
        render_outline_rect(r, rect_at(ox, oy, x + 4.0f, y + 9.0f, w - 8.0f, 5.0f), COL_DUMMY_DARK); // target ring
        if (d->hp < d->max_hp) {
            draw_hp_bar(r, rect_at(ox, oy, x, y - 5.0f, w, 3.0f), (f32)d->hp / (f32)d->max_hp);
        }
    }
}

static void draw_projectiles(Render *r, const GameState *gs, f32 alpha, i32 ox, i32 oy) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type != ENT_PROJECTILE) {
            continue;
        }
        const f32 x = roundf(lerpf(e->prev_pos.x, e->body.pos.x, alpha));
        const f32 y = roundf(lerpf(e->prev_pos.y, e->body.pos.y, alpha));
        const f32 w = e->body.size.x, h = e->body.size.y;
        const WeaponKind wk = e->as.projectile.weapon;
        switch (weapon_def(wk)->shot) {
        case SHOT_GRENADE:
            fill_circle(r, vec2(x + w * 0.5f - (f32)ox, y + h * 0.5f - (f32)oy), w * 0.5f, WEAPON_COLORS[wk]);
            break;
        case SHOT_ROCKET: {
            const Vec2 c = vec2(x + w * 0.5f - (f32)ox, y + h * 0.5f - (f32)oy);
            const f32 sp = sqrtf(e->body.vel.x * e->body.vel.x + e->body.vel.y * e->body.vel.y);
            const Vec2 back = sp > 0.0f ? vec2_scale(e->body.vel, -7.0f / sp) : vec2(0.0f, 0.0f); // exhaust behind
            render_fill_rect(r, rect_at(ox, oy, x, y, w, h), WEAPON_COLORS[wk]);
            render_line(r, c, vec2_add(c, back), COL_BULLET);
            break;
        }
        default:
            render_fill_rect(r, rect_at(ox, oy, x, y, w, h), COL_BULLET);
            break;
        }
    }
}

static void draw_fx(Render *r, const FxList *fx, i32 ox, i32 oy) {
    if (!fx) {
        return;
    }
    for (u32 i = 0; i < fx->count; i++) {
        const Fx *f = &fx->items[i];
        const f32 t = fx_progress(f);
        const Vec2 c = vec2(f->pos.x - (f32)ox, f->pos.y - (f32)oy);
        switch (f->kind) {
        case FX_EXPLOSION:
            fill_circle(r, c, f->radius * (0.35f + 0.65f * t), (Color){255, (u8)lerpf(120.0f, 40.0f, t), 30, 255});
            fill_circle(r, c, f->radius * (0.2f + 0.45f * t), (Color){255, 240, (u8)lerpf(160.0f, 60.0f, t), 255});
            break;
        case FX_SPARK:
            render_fill_rect(r, (Rect){c.x - f->radius * (1.0f - t), c.y - 1.0f, f->radius * 2.0f * (1.0f - t), 2.0f}, COL_BULLET);
            render_fill_rect(r, (Rect){c.x - 1.0f, c.y - f->radius * (1.0f - t), 2.0f, f->radius * 2.0f * (1.0f - t)}, COL_BULLET);
            break;
        case FX_MUZZLE:
            fill_circle(r, c, f->radius, COL_FLASH);
            break;
        }
    }
}

static void draw_crosshair(Render *r, const Vec2 *at) {
    if (!at) {
        return;
    }
    const Color black = {0, 0, 0, 255};
    const f32 x = roundf(at->x), y = roundf(at->y);
    render_line(r, vec2(x - 5.0f, y + 1.0f), vec2(x + 5.0f, y + 1.0f), black); // drop shadow
    render_line(r, vec2(x + 1.0f, y - 5.0f), vec2(x + 1.0f, y + 5.0f), black);
    render_line(r, vec2(x - 5.0f, y), vec2(x - 2.0f, y), COL_FLASH);
    render_line(r, vec2(x + 2.0f, y), vec2(x + 5.0f, y), COL_FLASH);
    render_line(r, vec2(x, y - 5.0f), vec2(x, y - 2.0f), COL_FLASH);
    render_line(r, vec2(x, y + 2.0f), vec2(x, y + 5.0f), COL_FLASH);
}

static const Color COL_TEXT = {236, 236, 244, 255};
static const Color COL_TEXT_DIM = {150, 154, 176, 255};
static const Color ORDER_COLORS[ORDER_COUNT] = {
    {80, 220, 230, 255}, {100, 210, 100, 255}, {240, 160, 50, 255}, {235, 70, 70, 255}, {190, 110, 240, 255}};

// The team the HUD is about: the controlled soldier's, else the human's.
static i32 hud_team(const GameState *gs) {
    return gs->controlled >= 0 ? gs->soldiers[gs->controlled].team : PLAYER_TEAM;
}

// Top left: the controlled soldier (hit points, weapon) and its team (order, squad).
static void draw_soldier_panel(Render *r, const GameState *gs) {
    const i32 team = hud_team(gs);
    if (gs->teams[team].ai) {
        ui_panel(r, (Rect){4.0f, 4.0f, 92.0f, 17.0f}, COL_MENU_BORDER);
        ui_text_shadow(r, 9.0f, 9.0f, "SPECTATEUR", COL_TEXT, 1, ALIGN_LEFT);
        return;
    }
    const Companion *c = game_player_const(gs);
    char line[48];
    ui_panel(r, (Rect){4.0f, 4.0f, 136.0f, 58.0f}, ui_team_color_dark(team));
    ui_text_shadow(r, 9.0f, 9.0f, "PV", COL_TEXT_DIM, 1, ALIGN_LEFT);
    draw_hp_bar(r, (Rect){24.0f, 9.0f, 64.0f, 7.0f}, c->max_hp > 0 ? (f32)c->hp / (f32)c->max_hp : 0.0f);
    snprintf(line, sizeof(line), "%d", MAX(c->hp, 0));
    ui_text_shadow(r, 134.0f, 9.0f, line, COL_TEXT, 1, ALIGN_RIGHT);

    render_fill_rect(r, (Rect){9.0f, 22.0f, 7.0f, 7.0f}, WEAPON_COLORS[c->weapon]);
    if (c->weapon == WEAPON_FISTS) {
        ui_text_shadow(r, 20.0f, 22.0f, ui_weapon_label(c->weapon), COL_TEXT, 1, ALIGN_LEFT);
    } else {
        snprintf(line, sizeof(line), "%s", ui_weapon_label(c->weapon));
        ui_text_shadow(r, 20.0f, 22.0f, line, COL_TEXT, 1, ALIGN_LEFT);
        snprintf(line, sizeof(line), "%d", c->ammo);
        ui_text_shadow(r, 134.0f, 22.0f, line, c->ammo * 4 <= weapon_def(c->weapon)->max_ammo ? COL_HP_LOW : COL_TEXT, 1,
                       ALIGN_RIGHT);
    }

    const Order order = gs->teams[team].order;
    ui_text_shadow(r, 9.0f, 35.0f, "ORDRE", COL_TEXT_DIM, 1, ALIGN_LEFT);
    ui_text_shadow(r, 44.0f, 35.0f, ui_order_label(order), ORDER_COLORS[order], 1, ALIGN_LEFT);

    // One pip per living soldier of the team (the controlled one is outlined), and their number.
    i32 n = 0;
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        if (gs->soldiers[i].alive && gs->soldiers[i].team == team) {
            if (n < 16) {
                const Rect pip = {10.0f + (f32)(n * 6), 49.0f, 4.0f, 7.0f};
                render_fill_rect(r, pip, ui_team_color(team));
                if (i == gs->controlled) {
                    render_outline_rect(r, (Rect){pip.x - 1.0f, pip.y - 1.0f, 6.0f, 9.0f}, COL_MARKER);
                }
            }
            n++;
        }
    }
    snprintf(line, sizeof(line), "%d", n);
    ui_text_shadow(r, 134.0f, 49.0f, line, COL_TEXT, 1, ALIGN_RIGHT);
}

// Top center: the hit points of every base.
static void draw_base_bars(Render *r, const GameState *gs) {
    i32 count = 0;
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        count += gs->teams[t].has_base ? 1 : 0;
    }
    if (count == 0) {
        return;
    }
    const f32 slot = 84.0f;
    f32 x = (f32)LOGICAL_W * 0.5f - slot * (f32)count * 0.5f;
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        const Team *team = &gs->teams[t];
        if (!team->has_base) {
            continue;
        }
        const Rect box = {x + 2.0f, 4.0f, slot - 4.0f, 17.0f};
        const bool flash = team->base_hurt_ticks > SECONDS_TO_TICKS(TUNING_BASE_ALERT_S) - 8;
        ui_panel(r, box, flash ? COL_FLASH : ui_team_color_dark(t));
        render_fill_rect(r, (Rect){box.x + 4.0f, box.y + 5.0f, 7.0f, 7.0f}, ui_team_color(t));
        if (team->base_hp > 0) {
            const f32 f = (f32)team->base_hp / (f32)TUNING_BASE_HP;
            render_fill_rect(r, (Rect){box.x + 15.0f, box.y + 6.0f, 56.0f, 5.0f}, COL_HP_BACK);
            render_fill_rect(r, (Rect){box.x + 15.0f, box.y + 6.0f, 56.0f * f, 5.0f}, ui_team_color(t));
        } else {
            ui_text_shadow(r, box.x + 15.0f, box.y + 5.0f, "DÉTRUITE", COL_TEXT_DIM, 1, ALIGN_LEFT);
        }
        x += slot;
    }
}

// Top right: how long the match has lasted.
static void draw_clock(Render *r, const GameState *gs) {
    const u64 ticks = gs->match.over ? gs->match.end_tick : gs->tick;
    const u64 s = ticks / TICK_HZ;
    char line[16];
    snprintf(line, sizeof(line), "%02u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
    ui_panel(r, (Rect){(f32)LOGICAL_W - 46.0f, 4.0f, 42.0f, 17.0f}, COL_MENU_BORDER);
    ui_text_shadow(r, (f32)LOGICAL_W - 25.0f, 9.0f, line, COL_TEXT, 1, ALIGN_CENTER);
}

// A marker on the edge of the screen in the direction of something out of view.
static void draw_edge_marker(Render *r, Vec2 world, i32 ox, i32 oy, Color color, bool square) {
    const f32 x = world.x - (f32)ox, y = world.y - (f32)oy;
    const f32 margin = 8.0f;
    if (x >= -4.0f && x <= (f32)LOGICAL_W + 4.0f && y >= -4.0f && y <= (f32)LOGICAL_H + 4.0f) {
        return; // on screen
    }
    const f32 cx = (f32)LOGICAL_W * 0.5f, cy = (f32)LOGICAL_H * 0.5f;
    const f32 dx = x - cx, dy = y - cy;
    const f32 kx = fabsf(dx) > 0.001f ? (cx - margin) / fabsf(dx) : 1e9f;
    const f32 ky = fabsf(dy) > 0.001f ? (cy - margin - 24.0f) / fabsf(dy) : 1e9f; // keep clear of the top panels
    const f32 k = MIN(kx, ky);
    const Vec2 m = vec2(cx + dx * k, cy + dy * k + (dy < 0.0f ? 12.0f : 0.0f));
    const f32 len = sqrtf(dx * dx + dy * dy);
    const Vec2 dir = vec2(dx / len, dy / len);
    const Color dark = {12, 12, 20, 255};
    const Vec2 tip = vec2_add(m, vec2_scale(dir, 7.0f));
    render_line(r, vec2_add(m, vec2(-dir.y * 3.0f, dir.x * 3.0f)), tip, color);
    render_line(r, vec2_add(m, vec2(dir.y * 3.0f, -dir.x * 3.0f)), tip, color);
    if (square) {
        render_fill_rect(r, (Rect){m.x - 3.0f, m.y - 3.0f, 6.0f, 6.0f}, color);
        render_outline_rect(r, (Rect){m.x - 4.0f, m.y - 4.0f, 8.0f, 8.0f}, dark);
    } else {
        fill_circle(r, m, 3.5f, color);
    }
}

#define MAX_CRATE_MARKERS 3
#define CRATE_MARKER_RANGE 1400.0f

// Out of view: every standing base (circle in the team color), and the closest free crates (squares).
static void draw_edge_markers(Render *r, const GameState *gs, i32 ox, i32 oy) {
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        if (base_alive(gs, t)) {
            const Rect b = base_rect(gs, t);
            draw_edge_marker(r, vec2(b.x + b.w * 0.5f, b.y + b.h * 0.5f), ox, oy, ui_team_color(t), false);
        }
    }
    const Vec2 me = body_center(&game_player_const(gs)->body);
    const Entity *best[MAX_CRATE_MARKERS] = {NULL};
    f32 best_d[MAX_CRATE_MARKERS] = {0};
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type != ENT_CRATE || e->pending_destroy || e->as.crate.state == CRATE_CARRIED) {
            continue;
        }
        bool on_pad = false;
        for (i32 t = 0; t < MAX_TEAMS; t++) {
            on_pad |= pad_holds_crate(gs, t, e);
        }
        const Vec2 c = body_center(&e->body);
        const f32 d = sqrtf((c.x - me.x) * (c.x - me.x) + (c.y - me.y) * (c.y - me.y));
        if (on_pad || d > CRATE_MARKER_RANGE) {
            continue;
        }
        for (i32 k = 0; k < MAX_CRATE_MARKERS; k++) { // insertion into the closest three
            if (!best[k] || d < best_d[k]) {
                for (i32 j = MAX_CRATE_MARKERS - 1; j > k; j--) {
                    best[j] = best[j - 1];
                    best_d[j] = best_d[j - 1];
                }
                best[k] = e;
                best_d[k] = d;
                break;
            }
        }
    }
    for (i32 k = 0; k < MAX_CRATE_MARKERS && best[k]; k++) {
        draw_edge_marker(r, body_center(&best[k]->body), ox, oy,
                         best[k]->as.crate.kind == CRATE_BLUE ? COL_CRATE_BLUE : COL_CRATE_RED, true);
    }
}

// Big centered message once the match is decided (the results screen follows).
static void draw_match_banner(Render *r, const GameState *gs) {
    if (!gs->match.over) {
        return;
    }
    const i32 human = PLAYER_TEAM;
    const char *text;
    Color color;
    if (gs->match.winner < 0) {
        text = "MATCH NUL";
        color = COL_TEXT;
    } else if (!gs->teams[human].ai && gs->match.winner == human) {
        text = "VICTOIRE !";
        color = COL_MENU_SELECTED;
    } else if (!gs->teams[human].ai) {
        text = "DÉFAITE";
        color = COL_HP_LOW;
    } else {
        text = ui_team_name(gs->match.winner);
        color = ui_team_color(gs->match.winner);
    }
    render_fill_rect(r, (Rect){0.0f, 150.0f, (f32)LOGICAL_W, 52.0f}, (Color){10, 10, 18, 190});
    ui_text_shadow(r, (f32)LOGICAL_W * 0.5f, 162.0f, text, color, 4, ALIGN_CENTER);
}

static void draw_hud(Render *r, const GameState *gs, i32 ox, i32 oy) {
    draw_edge_markers(r, gs, ox, oy);
    draw_soldier_panel(r, gs);
    draw_base_bars(r, gs);
    draw_clock(r, gs);
    if (gs->controlled < 0 && !gs->teams[PLAYER_TEAM].ai && !gs->match.over) { // the whole team is gone
        render_fill_rect(r, (Rect){0.0f, 0.0f, (f32)LOGICAL_W, (f32)LOGICAL_H}, (Color){10, 0, 0, 150});
    }
    draw_match_banner(r, gs);
}

// Mini picture of the shape: one 5 px square per consumed crate, in that crate's color.
static void draw_menu_entry(Render *r, const PadMenu *m, i32 index, i32 ox, i32 oy) {
    const PadOption *o = &m->options.items[index];
    const Rect er = m->rects[index];
    const bool selected = index == m->selected;
    const Rect box = rect_at(ox, oy, er.x, er.y, er.w, er.h);
    render_fill_rect(r, box, COL_MENU_BG);
    render_outline_rect(r, box, selected ? COL_MENU_SELECTED : COL_MENU_BORDER);
    if (selected) {
        render_outline_rect(r, (Rect){box.x + 1.0f, box.y + 1.0f, box.w - 2.0f, box.h - 2.0f}, COL_MENU_SELECTED);
    }
    const f32 cell = 5.0f;
    const f32 ix = box.x + 5.0f, iy = box.y + (box.h - 2.0f * cell) * 0.5f;
    for (u32 k = 0; k < o->count; k++) {
        const PadCoord c = o->cells[k];
        const bool blue = m->grid.cells[c.row][c.col] == PAD_BLUE;
        render_fill_rect(r, (Rect){ix + (f32)c.col * cell, iy + (f32)c.row * cell, cell - 1.0f, cell - 1.0f},
                         blue ? COL_CRATE_BLUE : COL_CRATE_RED);
    }
    const Color pc = PRODUCT_COLORS[o->product];
    const Rect sw = {box.x + box.w - 15.0f, box.y + 3.0f, 12.0f, box.h - 6.0f};
    render_fill_rect(r, sw, pc);
    render_outline_rect(r, sw, COL_WOOD_DARK);
}

static void draw_pad_menu(Render *r, const PadMenu *m, i32 ox, i32 oy) {
    if (!m || !m->visible) {
        return;
    }
    for (u32 i = 0; i < m->options.count; i++) {
        draw_menu_entry(r, m, (i32)i, ox, oy);
    }
    if (m->selected >= 0 && (u32)m->selected < m->options.count) { // the name of the selected construction
        const Rect er = m->rects[m->selected];
        f32 top = er.y;
        for (u32 i = 0; i < m->options.count; i++) {
            top = MIN(top, m->rects[i].y);
        }
        ui_text_shadow(r, er.x - (f32)ox, top - 11.0f - (f32)oy, ui_product_label(m->options.items[m->selected].product),
                       COL_MENU_SELECTED, 1, ALIGN_LEFT);
    }
}

static void draw_flag(Render *r, int slot, bool on, Color color) {
    const Color off = {40, 40, 56, 255};
    render_fill_rect(r, (Rect){4.0f + (f32)(slot * 9), 4.0f, 7.0f, 7.0f}, on ? color : off);
}

static void draw_debug(Render *r, const GameState *gs, f32 alpha, i32 ox, i32 oy) {
    const Level *lv = gs->level;
    const Companion *c = game_player_const(gs);

    // Tile grid.
    const Color grid = {52, 58, 88, 255};
    for (i32 tx = 0; tx <= lv->width; tx++) {
        f32 x = (f32)(tx * TILE_SIZE - ox);
        if (x >= 0 && x <= (f32)LOGICAL_W) {
            render_line(r, vec2(x, 0), vec2(x, (f32)LOGICAL_H), grid);
        }
    }
    for (i32 ty = 0; ty <= lv->height; ty++) {
        f32 y = (f32)(ty * TILE_SIZE - oy);
        if (y >= 0 && y <= (f32)LOGICAL_H) {
            render_line(r, vec2(0, y), vec2((f32)LOGICAL_W, y), grid);
        }
    }

    // Spawns (blue) and drop zones (orange).
    for (int i = 0; i < MAX_TEAMS; i++) {
        if (lv->has_spawn[i]) {
            Vec2 s = lv->spawns[i];
            render_fill_rect(r, rect_at(ox, oy, s.x - 4.0f, s.y - 10.0f, 8.0f, 10.0f),
                             (Color){70, 140, 255, 255});
        }
    }
    for (i32 i = 0; i < lv->drop_zone_count; i++) {
        Vec2 d = lv->drop_zones[i];
        render_fill_rect(r, rect_at(ox, oy, d.x - 1.0f, d.y, 2.0f, 40.0f),
                         (Color){255, 160, 40, 255});
    }

    // Hitbox and velocity.
    const f32 x = roundf(lerpf(c->prev_pos.x, c->body.pos.x, alpha));
    const f32 y = roundf(lerpf(c->prev_pos.y, c->body.pos.y, alpha));
    render_outline_rect(r, rect_at(ox, oy, x, y, c->body.size.x, c->body.size.y),
                        (Color){60, 255, 90, 255});
    Vec2 center = vec2(x + c->body.size.x * 0.5f - (f32)ox, y + c->body.size.y * 0.5f - (f32)oy);
    render_line(r, center, vec2_add(center, vec2_scale(c->body.vel, 0.2f)),
                (Color){80, 230, 255, 255});

    // State flags: ground, ladder, coyote, jump buffer, drop-through, rising.
    draw_flag(r, 0, c->body.on_ground, (Color){70, 220, 90, 255});
    draw_flag(r, 1, c->on_ladder, (Color){240, 220, 60, 255});
    draw_flag(r, 2, c->coyote_ticks > 0, (Color){70, 220, 220, 255});
    draw_flag(r, 3, c->jump_buffer_ticks > 0, (Color){230, 80, 220, 255});
    draw_flag(r, 4, c->drop_ticks > 0, (Color){240, 240, 240, 255});
    draw_flag(r, 5, c->is_jumping, (Color){255, 150, 40, 255});
}

static const Color MODE_COLORS[AI_MODE_COUNT] = {
    {120, 120, 130, 255}, // idle
    {80, 220, 230, 255},  // follow
    {100, 210, 100, 255}, // guard
    {240, 160, 50, 255},  // fetch
    {255, 230, 90, 255},  // deliver
    {190, 110, 240, 255}, // steal
    {235, 70, 70, 255},   // attack
    {240, 240, 240, 255}, // arm
    {255, 120, 170, 255}, // flee
};

// Navigation graph: a dot per standing place, a line per move (blue walk, orange jump, pink drop, yellow climb);
// over it, the path of each AI soldier in white and a square above its head in the color of what it is doing.
static void draw_nav(Render *r, const GameState *gs, i32 ox, i32 oy) {
    const Nav *nav = &gs->nav;
    if (nav->ok) {
        static const Color KIND_COLORS[4] = {{70, 110, 190, 255}, {235, 150, 50, 255}, {235, 90, 200, 255}, {240, 220, 60, 255}};
        for (u32 i = 0; i < nav->edge_count; i++) {
            const NavEdge *e = &nav->edges[i];
            const Vec2 a = nav_node_feet(nav, e->from), b = nav_node_feet(nav, e->to);
            const f32 ax = a.x - (f32)ox, ay = a.y - 4.0f - (f32)oy, bx = b.x - (f32)ox, by = b.y - 4.0f - (f32)oy;
            if ((ax < 0 && bx < 0) || (ax > LOGICAL_W && bx > LOGICAL_W) || (ay < 0 && by < 0) ||
                (ay > LOGICAL_H && by > LOGICAL_H)) {
                continue;
            }
            render_line(r, vec2(ax, ay), vec2(bx, by), KIND_COLORS[e->kind]);
        }
        for (u32 i = 0; i < nav->node_count; i++) {
            const Vec2 p = nav_node_feet(nav, (i32)i);
            render_fill_rect(r, (Rect){p.x - 1.0f - (f32)ox, p.y - 5.0f - (f32)oy, 3.0f, 3.0f},
                             nav->nodes[i].kind == NAV_LADDER ? (Color){240, 220, 60, 255} : (Color){110, 240, 130, 255});
        }
    }
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        const Soldier *s = &gs->soldiers[i];
        if (!s->alive || (i == gs->controlled && !gs->teams[s->team].ai)) {
            continue;
        }
        const Vec2 c = body_center(&s->c.body);
        render_fill_rect(r, (Rect){c.x - 3.0f - (f32)ox, s->c.body.pos.y - 9.0f - (f32)oy, 6.0f, 5.0f}, MODE_COLORS[s->ai.mode]);
        if (nav->ok) {
            for (u32 k = s->ai.nav.path_pos; k < s->ai.nav.path_len; k++) {
                const NavEdge *e = &nav->edges[s->ai.nav.path[k]];
                const Vec2 a = nav_node_feet(nav, e->from), b = nav_node_feet(nav, e->to);
                render_line(r, vec2(a.x - (f32)ox, a.y - 6.0f - (f32)oy), vec2(b.x - (f32)ox, b.y - 6.0f - (f32)oy),
                            (Color){245, 245, 245, 255});
            }
        }
    }
}

void draw_game(Render *r, const GameState *gs, const Camera *cam, const PadMenu *menu, const FxList *fx,
               const Vec2 *crosshair, f32 alpha, u32 flags) {
    const Vec2 cp = camera_interpolated(cam, alpha);
    const i32 ox = (i32)roundf(cp.x);
    const i32 oy = (i32)roundf(cp.y);

    render_begin(r, (Color){24, 28, 44, 255});
    draw_background(r, ox, oy);
    draw_tiles(r, gs->level, ox, oy, gs->tick);
    draw_bases(r, gs, ox, oy);
    draw_pads(r, gs, ox, oy);
    draw_crates(r, gs, alpha, ox, oy, flags);
    draw_products(r, gs, alpha, ox, oy);
    draw_loot_weapons(r, gs, alpha, ox, oy);
    draw_dummies(r, gs, alpha, ox, oy);
    draw_soldiers(r, gs, alpha, ox, oy);
    draw_projectiles(r, gs, alpha, ox, oy);
    draw_fx(r, fx, ox, oy);
    draw_pad_menu(r, menu, ox, oy);
    draw_hud(r, gs, ox, oy);
    draw_crosshair(r, crosshair);
    if (flags & DRAW_NAV) {
        draw_nav(r, gs, ox, oy);
    }
    if (flags & DRAW_DEBUG) {
        draw_debug(r, gs, alpha, ox, oy);
    }
}
