#include "render/draw_game.h"

#include "core/config.h"
#include "game/tuning.h"

#include <math.h>

// Placeholder palette (final art comes later).
static const Color COL_SOLID = {74, 62, 92, 255};
static const Color COL_SOLID_TOP = {118, 104, 142, 255};
static const Color COL_SOLID_SHADE = {58, 48, 74, 255};
static const Color COL_PLATFORM = {176, 134, 72, 255};
static const Color COL_PLATFORM_DARK = {120, 90, 48, 255};
static const Color COL_LADDER = {156, 116, 64, 255};
static const Color COL_WATER = {36, 96, 190, 255};
static const Color COL_WATER_TOP = {110, 170, 240, 255};
static const Color COL_PLAYER = {230, 90, 70, 255};
static const Color COL_PLAYER_DARK = {150, 50, 40, 255};
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

static void draw_background(Render *r) {
    // Vertical dusk gradient in a few bands.
    const int bands = 9;
    const int band_h = (LOGICAL_H + bands - 1) / bands;
    for (int i = 0; i < bands; i++) {
        f32 t = (f32)i / (f32)(bands - 1);
        Color c = {(u8)lerpf(24.0f, 72.0f, t), (u8)lerpf(28.0f, 56.0f, t),
                   (u8)lerpf(52.0f, 96.0f, t), 255};
        render_fill_rect(r, (Rect){0, (f32)(i * band_h), (f32)LOGICAL_W, (f32)band_h}, c);
    }
}

static void draw_one_way_top(Render *r, f32 x, f32 y) {
    const f32 t = (f32)TILE_SIZE;
    render_fill_rect(r, (Rect){x, y, t, 5.0f}, COL_PLATFORM);
    render_fill_rect(r, (Rect){x, y + 5.0f, t, 1.0f}, COL_PLATFORM_DARK);
}

static void draw_tile(Render *r, const Level *lv, i32 tx, i32 ty, f32 x, f32 y) {
    const f32 t = (f32)TILE_SIZE;
    switch (level_tile(lv, tx, ty)) {
    case TILE_SOLID:
        render_fill_rect(r, (Rect){x, y, t, t}, COL_SOLID);
        render_fill_rect(r, (Rect){x, y + t - 2.0f, t, 2.0f}, COL_SOLID_SHADE);
        if (level_tile(lv, tx, ty - 1) != TILE_SOLID) {
            render_fill_rect(r, (Rect){x, y, t, 3.0f}, COL_SOLID_TOP);
        }
        break;
    case TILE_PLATFORM:
        draw_one_way_top(r, x, y);
        break;
    case TILE_LADDER:
        render_fill_rect(r, (Rect){x + 3.0f, y, 2.0f, t}, COL_LADDER);
        render_fill_rect(r, (Rect){x + 11.0f, y, 2.0f, t}, COL_LADDER);
        for (int i = 0; i < 4; i++) {
            render_fill_rect(r, (Rect){x + 5.0f, y + 2.0f + (f32)(i * 4), 6.0f, 1.0f},
                             COL_LADDER);
        }
        if (level_is_one_way(lv, tx, ty)) {
            draw_one_way_top(r, x, y);
        }
        break;
    case TILE_HAZARD:
        render_fill_rect(r, (Rect){x, y, t, t}, COL_WATER);
        if (level_tile(lv, tx, ty - 1) != TILE_HAZARD) {
            render_fill_rect(r, (Rect){x, y, t, 2.0f}, COL_WATER_TOP);
        }
        break;
    default:
        break;
    }
}

static void draw_tiles(Render *r, const Level *lv, i32 ox, i32 oy) {
    i32 tx0 = MAX(0, ox / TILE_SIZE);
    i32 ty0 = MAX(0, oy / TILE_SIZE);
    i32 tx1 = MIN(lv->width - 1, (ox + LOGICAL_W) / TILE_SIZE);
    i32 ty1 = MIN(lv->height - 1, (oy + LOGICAL_H) / TILE_SIZE);
    for (i32 ty = ty0; ty <= ty1; ty++) {
        for (i32 tx = tx0; tx <= tx1; tx++) {
            draw_tile(r, lv, tx, ty, (f32)(tx * TILE_SIZE - ox), (f32)(ty * TILE_SIZE - oy));
        }
    }
}

static void draw_player(Render *r, const Companion *c, f32 alpha, i32 ox, i32 oy) {
    const f32 x = roundf(lerpf(c->prev_pos.x, c->body.pos.x, alpha));
    const f32 y = roundf(lerpf(c->prev_pos.y, c->body.pos.y, alpha));
    const f32 w = c->body.size.x, h = c->body.size.y;
    const bool flash = c->hurt_ticks > 0 && (c->hurt_ticks / 2) % 2 == 0;
    render_fill_rect(r, rect_at(ox, oy, x, y, w, h), flash ? COL_FLASH : COL_PLAYER);
    render_fill_rect(r, rect_at(ox, oy, x, y + h - 4.0f, w, 4.0f), flash ? COL_FLASH : COL_PLAYER_DARK);
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

// The 4x2 area of every pad: a plate under it and faint cell outlines.
static void draw_pads(Render *r, const GameState *gs, i32 ox, i32 oy) {
    for (i32 team = 0; team < MAX_TEAMS; team++) {
        if (!gs->level->pads[team].present) {
            continue;
        }
        const Rect pr = pad_rect(gs, team);
        render_fill_rect(r, rect_at(ox, oy, pr.x - 2.0f, pr.y + pr.h, pr.w + 4.0f, 3.0f), COL_PAD_PLATE);
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

// Hit points and ammunition of the controlled soldier, as bars (there is no text rendering yet).
static void draw_hud(Render *r, const Companion *c) {
    draw_hp_bar(r, (Rect){4.0f, 14.0f, 64.0f, 6.0f}, (f32)c->hp / (f32)c->max_hp);
    if (c->weapon != WEAPON_FISTS) {
        const WeaponDef *def = weapon_def(c->weapon);
        const f32 f = (f32)c->ammo / (f32)def->max_ammo;
        render_fill_rect(r, (Rect){4.0f, 22.0f, 64.0f, 4.0f}, COL_HP_BACK);
        render_fill_rect(r, (Rect){4.0f, 22.0f, 64.0f * clampf(f, 0.0f, 1.0f), 4.0f}, WEAPON_COLORS[c->weapon]);
    }
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
}

static void draw_flag(Render *r, int slot, bool on, Color color) {
    const Color off = {40, 40, 56, 255};
    render_fill_rect(r, (Rect){4.0f + (f32)(slot * 9), 4.0f, 7.0f, 7.0f}, on ? color : off);
}

static void draw_debug(Render *r, const GameState *gs, f32 alpha, i32 ox, i32 oy) {
    const Level *lv = gs->level;
    const Companion *c = &gs->player;

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

void draw_game(Render *r, const GameState *gs, const Camera *cam, const PadMenu *menu, const FxList *fx,
               const Vec2 *crosshair, f32 alpha, u32 flags) {
    const Vec2 cp = camera_interpolated(cam, alpha);
    const i32 ox = (i32)roundf(cp.x);
    const i32 oy = (i32)roundf(cp.y);

    render_begin(r, (Color){24, 28, 44, 255});
    draw_background(r);
    draw_tiles(r, gs->level, ox, oy);
    draw_pads(r, gs, ox, oy);
    draw_crates(r, gs, alpha, ox, oy, flags);
    draw_products(r, gs, alpha, ox, oy);
    draw_loot_weapons(r, gs, alpha, ox, oy);
    draw_dummies(r, gs, alpha, ox, oy);
    draw_player(r, &gs->player, alpha, ox, oy);
    draw_projectiles(r, gs, alpha, ox, oy);
    draw_fx(r, fx, ox, oy);
    draw_pad_menu(r, menu, ox, oy);
    draw_hud(r, &gs->player);
    draw_crosshair(r, crosshair);
    if (flags & DRAW_DEBUG) {
        draw_debug(r, gs, alpha, ox, oy);
    }
}
