#include "render/draw_game.h"

#include "core/config.h"
#include "game/base.h"
#include "game/combat.h"
#include "game/crate.h"
#include "game/loot.h"
#include "game/squad.h"
#include "game/tuning.h"
#include "render/draw_ui.h"

#include <stdio.h>

#include <math.h>

// Colors of what is still drawn with shapes.
static const Color COL_WATER = {36, 96, 190, 255};
static const Color COL_WATER_TOP = {110, 170, 240, 255};
static const Color COL_CRATE_RED = {208, 64, 56, 255};
static const Color COL_CRATE_BLUE = {56, 112, 226, 255};

static const Color COL_PAD_PLATE = {70, 190, 170, 255};
static const Color COL_MENU_BG = {20, 22, 36, 235};
static const Color COL_MENU_BORDER = {120, 126, 160, 255};
static const Color COL_MENU_SELECTED = {255, 224, 80, 255};

static const Color COL_HP_BACK = {40, 20, 24, 255};
static const Color COL_HP = {90, 220, 100, 255};
static const Color COL_HP_LOW = {230, 70, 60, 255};
static const Color COL_FLASH = {255, 255, 255, 255};
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
static const Color COL_PLANK_DARK = {120, 84, 44, 255};

static void tile_sprite(Render *r, SpriteId id, f32 x, f32 y, u8 darken) {
    const SpriteDraw d = {.id = id, .pos = vec2(x, y), .darken = darken};
    render_sprite(r, &d);
}

// Now and then a tuft, a flower or a stone on top of the grass (fixed per tile).
static void draw_grass_deco(Render *r, u32 h, f32 x, f32 y) {
    const u32 roll = (h >> 16) % 16;
    const f32 at = x + 4.0f + (f32)((h >> 20) % 9);
    SpriteId id = SPR_COUNT;
    if (roll < 4) {
        id = SPR_DECO_TUFT;
    } else if (roll == 4) {
        id = SPR_DECO_FLOWER;
    } else if (roll == 5) {
        id = SPR_DECO_DAISY;
    } else if (roll == 6) {
        id = SPR_DECO_ROCK;
    }
    if (id != SPR_COUNT) {
        const SpriteDraw d = {.id = id, .pos = vec2(at, y + 1.0f), .flip_x = (h >> 25) & 1u};
        render_sprite(r, &d);
    }
}

// The ground right under a pad (kept clear of plants).
static bool under_pad(const Level *lv, i32 tx, i32 ty) {
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        const PadSite *p = &lv->pads[t];
        if (p->present && ty == p->cell_y + 1 && tx >= p->cell_x && tx < p->cell_x + PAD_COLS) {
            return true;
        }
    }
    return false;
}

#define EARTH_DARK_DEPTH 6 // tiles below the surface where the earth stops darkening
#define EARTH_DARK_STEP 16 // darkening per tile of depth (out of 255)

// Earth: grass on the top layer, then dirt that darkens with depth. Light on the left faces, shade on the right and
// under overhangs.
static void draw_solid(Render *r, const Level *lv, i32 tx, i32 ty, f32 x, f32 y) {
    const f32 t = (f32)TILE_SIZE;
    i32 depth = 0;
    while (depth < EARTH_DARK_DEPTH && level_tile(lv, tx, ty - depth - 1) == TILE_SOLID) {
        depth++;
    }
    const u32 h = tile_hash(tx, ty);
    const i32 variant = (i32)(h % SPRITE_TILE_VARIANTS);
    const bool grass = depth == 0 && level_tile(lv, tx, ty - 1) != TILE_HAZARD;
    tile_sprite(r, (SpriteId)((grass ? SPR_TILE_GRASS0 : SPR_TILE_DIRT0) + variant), x, y,
                (u8)(depth * EARTH_DARK_STEP));
    if (depth < EARTH_DARK_DEPTH) { // half a step more on the lower half: softer bands
        render_fill_rect(r, (Rect){x, y + t * 0.5f, t, t * 0.5f}, (Color){0, 0, 0, EARTH_DARK_STEP / 2});
    }
    const u8 edge = (u8)(70 - depth * 8);
    if (level_tile(lv, tx - 1, ty) != TILE_SOLID) {
        render_fill_rect(r, (Rect){x, y, 1.0f, t}, (Color){255, 230, 210, edge});
    }
    if (level_tile(lv, tx + 1, ty) != TILE_SOLID) {
        render_fill_rect(r, (Rect){x + t - 1.0f, y, 1.0f, t}, (Color){20, 10, 20, (u8)(edge + 40)});
    }
    if (level_tile(lv, tx, ty + 1) == TILE_EMPTY) {
        render_fill_rect(r, (Rect){x, y + t - 2.0f, t, 2.0f}, (Color){20, 10, 20, 90});
    }
    if (!grass) {
        return;
    }
    if (level_tile(lv, tx - 1, ty) == TILE_EMPTY) { // the grass hangs a little over the edges
        render_fill_rect(r, (Rect){x - 1.0f, y, 1.0f, 4.0f}, COL_GRASS);
        render_fill_rect(r, (Rect){x - 1.0f, y + 4.0f, 1.0f, 2.0f}, COL_GRASS_DARK);
    }
    if (level_tile(lv, tx + 1, ty) == TILE_EMPTY) {
        render_fill_rect(r, (Rect){x + t, y, 1.0f, 4.0f}, COL_GRASS);
        render_fill_rect(r, (Rect){x + t, y + 4.0f, 1.0f, 2.0f}, COL_GRASS_DARK);
    }
    if (level_tile(lv, tx, ty - 1) == TILE_EMPTY && !under_pad(lv, tx, ty)) {
        draw_grass_deco(r, h, x, y);
    }
}

// Water darkens with depth; the surface rolls and glints, with foam against the shore.
static void draw_water(Render *r, const Level *lv, i32 tx, i32 ty, f32 x, f32 y, u64 tick) {
    const f32 t = (f32)TILE_SIZE;
    i32 depth = 0;
    while (depth < 3 && level_tile(lv, tx, ty - depth - 1) == TILE_HAZARD) {
        depth++;
    }
    render_fill_rect(r, (Rect){x, y, t, t}, shade(COL_WATER, 1.0f - 0.12f * (f32)depth));
    if (depth > 0) {
        if ((tile_hash(tx, ty) + (u32)(tick / 20)) % 7 == 0) { // a bubble now and then
            render_fill_rect(r, (Rect){x + 6.0f, y + 6.0f - (f32)((tick / 4) % 6), 2.0f, 2.0f}, COL_WATER_TOP);
        }
        return;
    }
    for (i32 sx = 0; sx < TILE_SIZE; sx += 2) { // a rolling surface line
        const f32 wx = (f32)(tx * TILE_SIZE + sx);
        const f32 wave = sinf(wx * 0.25f + (f32)tick * 0.07f) + 0.5f * sinf(wx * 0.11f - (f32)tick * 0.05f);
        const f32 top = wave > 0.6f ? 0.0f : 1.0f;
        render_fill_rect(r, (Rect){x + (f32)sx, y + top, 2.0f, 3.0f - top}, COL_WATER_TOP);
    }
    const f32 phase = (f32)tick * 0.08f + (f32)tx * 1.3f;
    render_fill_rect(r, (Rect){x + 4.0f + roundf(sinf(phase) * 3.0f), y + 5.0f, 5.0f, 1.0f}, COL_WATER_TOP);
    if (((u32)(tick / 12) + tile_hash(tx, ty)) % 9 == 0) { // a glint
        render_fill_rect(r, (Rect){x + (f32)(tile_hash(tx, ty) % 12) + 2.0f, y + 8.0f, 2.0f, 1.0f}, COL_FLASH);
    }
    for (int side = -1; side <= 1; side += 2) { // foam where the water meets the ground
        if (level_tile(lv, tx + side, ty) == TILE_SOLID) {
            const f32 fx = side < 0 ? x : x + t - 3.0f;
            const f32 bob = (f32)((tick / 10 + (u32)tx) % 2);
            render_fill_rect(r, (Rect){fx, y - bob, 3.0f, 2.0f}, COL_FLASH);
        }
    }
}

static void draw_tile(Render *r, const Level *lv, i32 tx, i32 ty, f32 x, f32 y, u64 tick) {
    switch (level_tile(lv, tx, ty)) {
    case TILE_SOLID:
        draw_solid(r, lv, tx, ty, x, y);
        break;
    case TILE_PLATFORM:
        tile_sprite(r, SPR_TILE_PLANK, x, y, 0);
        if (level_tile(lv, tx, ty + 1) == TILE_EMPTY && (level_tile(lv, tx - 1, ty) != TILE_PLATFORM ||
                                                         level_tile(lv, tx + 1, ty) != TILE_PLATFORM)) {
            render_fill_rect(r, (Rect){x + 6.0f, y + 6.0f, 3.0f, 4.0f}, COL_PLANK_DARK); // end bracket
        }
        break;
    case TILE_LADDER:
        tile_sprite(r, SPR_TILE_LADDER, x, y, 0);
        if (level_is_one_way(lv, tx, ty)) {
            tile_sprite(r, SPR_TILE_PLANK, x, y, 0);
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

static const Color COL_MARKER = {255, 224, 80, 255};

static f32 deg(Vec2 dir) {
    return atan2f(dir.y, dir.x) * 180.0f / 3.14159265f;
}

static void sprite_at(Render *r, SpriteId id, i32 palette, Vec2 pos, f32 angle, bool flip_x, bool flip_y) {
    const SpriteDraw d = {.id = id, .palette = palette, .pos = pos, .angle = angle, .flip_x = flip_x, .flip_y = flip_y};
    render_sprite(r, &d);
}

// An arm from the shoulder along `dir` (drawn mirrored when it points left, so the sleeve stays on top).
static void draw_arm(Render *r, i32 palette, Vec2 shoulder, Vec2 dir) {
    sprite_at(r, SPR_ARM, palette, shoulder, deg(dir), false, dir.x < 0.0f);
}

// Both arms up along the sides of the head, stretched to reach the crate held overhead.
static void draw_carry_arm(Render *r, i32 palette, Vec2 shoulder, f32 side) {
    const SpriteDraw d = {.id = SPR_ARM,
                          .palette = palette,
                          .pos = vec2(shoulder.x + side * 5.0f, shoulder.y - 2.0f),
                          .angle = side > 0.0f ? -100.0f : -80.0f,
                          .scale_x = 1.6f,
                          .flip_y = side < 0.0f};
    render_sprite(r, &d);
}

static SpriteId weapon_sprite(WeaponKind w) {
    switch (w) {
    case WEAPON_PISTOL:
        return SPR_PISTOL;
    case WEAPON_UZI:
        return SPR_UZI;
    case WEAPON_GRENADES:
    case WEAPON_STRONG_GRENADES:
        return SPR_GRENADE;
    case WEAPON_ROCKET_LAUNCHER:
        return SPR_ROCKET_LAUNCHER;
    default:
        return SPR_COUNT;
    }
}

// The free arm hangs and swings with the stride; in the air it flails.
static Vec2 idle_arm_dir(BodyPose pose, f32 facing) {
    switch (pose) {
    case POSE_RUN0:
        return vec2(facing * 0.8f, 1.0f);
    case POSE_RUN2:
        return vec2(-facing * 0.6f, 1.0f);
    case POSE_JUMP:
        return vec2(facing * 0.6f, -1.0f);
    case POSE_FALL:
        return vec2(facing * 1.0f, -0.4f);
    default:
        return vec2(facing * 0.3f, 1.0f);
    }
}

static void draw_soldier(Render *r, const GameState *gs, const FxList *fx, i32 index, f32 alpha, i32 ox, i32 oy) {
    const Soldier *s = &gs->soldiers[index];
    const Companion *c = &s->c;
    const f32 x = roundf(lerpf(c->prev_pos.x, c->body.pos.x, alpha));
    const f32 y = roundf(lerpf(c->prev_pos.y, c->body.pos.y, alpha));
    const f32 w = c->body.size.x, h = c->body.size.y;
    const Vec2 feet = vec2(x + w * 0.5f - (f32)ox, y + h - (f32)oy);
    const BodyPose pose = fx ? fx_pose(fx, gs, index) : POSE_IDLE0;
    const f32 squash = fx ? fx_squash(fx, index) : 0.0f;
    const f32 sx = 1.0f + 0.2f * squash, sy = 1.0f - 0.18f * squash;
    const bool flash = c->hurt_ticks > 0 && (c->hurt_ticks / 2) % 2 == 0;
    const i32 palette = flash ? SPRITE_PALETTE_FLASH : s->team;
    const f32 facing = c->facing < 0 ? -1.0f : 1.0f;
    const f32 bob = (f32)sprite_pose_bob(pose);
    const Vec2 shoulder = vec2(feet.x - facing, feet.y - ((f32)(SPRITE_BODY_H - SPRITE_SHOULDER_Y) - bob) * sy);
    const bool carrying = entity_id_valid(c->carrying);

    // Behind the body: the other arm, when both hands are busy.
    if (carrying) {
        draw_carry_arm(r, palette, shoulder, -facing);
    } else if (c->on_ladder) {
        draw_arm(r, palette, shoulder, vec2(facing * 0.2f, pose == POSE_CLIMB0 ? -1.0f : -0.5f));
    }
    const SpriteDraw body = {.id = sprite_body((i32)s->variant, pose),
                             .palette = palette,
                             .pos = feet,
                             .scale_x = sx,
                             .scale_y = sy,
                             .flip_x = facing < 0.0f};
    render_sprite(r, &body);

    // In front: the arm that holds, punches or swings.
    if (carrying) {
        draw_carry_arm(r, palette, shoulder, facing);
    } else if (c->on_ladder) {
        draw_arm(r, palette, shoulder, vec2(facing * 0.2f, pose == POSE_CLIMB0 ? -0.5f : -1.0f));
    } else if (c->weapon == WEAPON_FISTS && c->punch_ticks > 0) {
        sprite_at(r, SPR_PUNCH, palette, shoulder, 0.0f, facing < 0.0f, false);
    } else if (c->weapon != WEAPON_FISTS) {
        const Vec2 dir = c->aim_dir;
        const SpriteId ws = weapon_sprite(c->weapon);
        draw_arm(r, palette, shoulder, dir);
        if (ws == SPR_GRENADE) {
            sprite_at(r, ws, palette, vec2_add(shoulder, vec2_scale(dir, 6.0f)), 0.0f, false, false);
        } else if (ws != SPR_COUNT) {
            const f32 reach = ws == SPR_ROCKET_LAUNCHER ? 2.0f : 5.0f;
            sprite_at(r, ws, palette, vec2_add(shoulder, vec2_scale(dir, reach)), deg(dir), false, dir.x < 0.0f);
        }
    } else {
        draw_arm(r, palette, shoulder, idle_arm_dir(pose, facing));
    }

    const f32 top = feet.y - (f32)SPRITE_BODY_H + bob;
    if (index == gs->controlled) { // a small arrow above the head
        const f32 mx = feet.x, my = top - 7.0f - (carrying ? TUNING_CRATE_SIZE : 0.0f);
        for (int k = 0; k < 4; k++) {
            render_line(r, vec2(mx - 3.0f + (f32)k, my + (f32)k), vec2(mx + 3.0f - (f32)k, my + (f32)k), COL_MARKER);
        }
    } else if (c->hp < c->max_hp) {
        draw_hp_bar(r, (Rect){feet.x - w * 0.5f, top - 5.0f, w, 3.0f}, (f32)c->hp / (f32)c->max_hp);
    }
}

static void draw_soldiers(Render *r, const GameState *gs, const FxList *fx, f32 alpha, i32 ox, i32 oy) {
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        if (gs->soldiers[i].alive) {
            draw_soldier(r, gs, fx, i, alpha, ox, oy);
        }
    }
}

static void draw_crates(Render *r, const GameState *gs, const FxList *fx, f32 alpha, i32 ox, i32 oy, u32 flags) {
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
        if (st == CRATE_DESCENDING) { // the parachute sways a little
            const f32 sway = sinf((f32)gs->tick * 0.07f + (f32)i) * 5.0f;
            sprite_at(r, SPR_PARACHUTE, 0, vec2(x + 8.0f, y - 5.0f), sway, false, false);
        }
        const f32 squash = fx ? fx_crate_squash(fx, i) : 0.0f; // wider and shorter for a moment after landing
        const SpriteDraw crate = {.id = e->as.crate.kind == CRATE_BLUE ? SPR_CRATE_BLUE : SPR_CRATE_RED,
                                  .pos = vec2(x + 8.0f, y + (f32)TUNING_CRATE_SIZE),
                                  .scale_x = 1.0f + 0.14f * squash,
                                  .scale_y = 1.0f - 0.22f * squash};
        render_sprite(r, &crate);
        if (flags & DRAW_DEBUG) { // outline by state: descending cyan, falling orange, carried magenta
            const Color outline = st == CRATE_DESCENDING ? (Color){80, 230, 255, 255}
                                  : st == CRATE_FALLING  ? (Color){255, 150, 40, 255}
                                  : st == CRATE_CARRIED  ? (Color){230, 80, 220, 255}
                                                         : (Color){60, 60, 80, 255};
            render_outline_rect(r, (Rect){x, y, TUNING_CRATE_SIZE, TUNING_CRATE_SIZE}, outline);
        }
    }
}

// A white frame around the crate the human player would take (or break) with a press, or, while carrying one, a frame on
// the tile where it would be set down (lightly filled: an empty slot). A dark line outside keeps it readable on any
// background.
static void draw_crate_frame(Render *r, Rect box, u8 fill) {
    const Color dark = {10, 8, 20, 190}, white = {255, 255, 255, 255};
    render_outline_rect(r, (Rect){box.x - 2.0f, box.y - 2.0f, box.w + 4.0f, box.h + 4.0f}, dark);
    if (fill > 0) {
        render_fill_rect(r, box, (Color){255, 255, 255, fill});
    }
    render_outline_rect(r, (Rect){box.x - 1.0f, box.y - 1.0f, box.w + 2.0f, box.h + 2.0f}, white);
}

static void draw_crate_cues(Render *r, const GameState *gs, i32 ox, i32 oy) {
    if (gs->controlled < 0 || gs->teams[gs->soldiers[gs->controlled].team].ai) {
        return; // spectating: nobody to guide
    }
    const Companion *p = game_player_const(gs);
    if (entity_id_valid(p->carrying)) {
        i32 col, row;
        if (crate_place_target(gs, p, &col, &row)) {
            draw_crate_frame(r, rect_at(ox, oy, (f32)(col * TILE_SIZE), (f32)(row * TILE_SIZE), (f32)TILE_SIZE, (f32)TILE_SIZE), 56);
        }
        return;
    }
    const Entity *e = pool_get_const(&gs->entities, crate_pickup_target(gs, p));
    if (e) {
        draw_crate_frame(r, rect_at(ox, oy, e->body.pos.x, e->body.pos.y, TUNING_CRATE_SIZE, TUNING_CRATE_SIZE), 0);
    }
}

// A bunker in the team colors with its flag; cracked when hurt, a heap of rubble once destroyed.
static void draw_base(Render *r, const GameState *gs, i32 team, i32 ox, i32 oy) {
    const Team *t = &gs->teams[team];
    const Rect b = base_rect(gs, team);
    const f32 x = b.x - (f32)ox, y = b.y - (f32)oy, w = b.w, h = b.h;
    if (x + w < 0.0f || x > (f32)LOGICAL_W || y + h + 8.0f < 0.0f || y - 16.0f > (f32)LOGICAL_H) {
        return;
    }
    const Vec2 foot = vec2(x + w * 0.5f, y + h);
    if (t->base_hp <= 0) {
        sprite_at(r, SPR_BASE_RUIN, team, foot, 0.0f, false, false);
        return;
    }
    const bool flash = t->base_hurt_ticks > SECONDS_TO_TICKS(TUNING_BASE_ALERT_S) - 6;
    const SpriteId id = t->base_hp * 4 < TUNING_BASE_HP   ? SPR_BASE_WRECKED
                        : t->base_hp * 2 < TUNING_BASE_HP ? SPR_BASE_DAMAGED
                                                          : SPR_BASE;
    const SpriteId flag = (gs->tick / 14 + (u64)team) % 2 ? SPR_FLAG1 : SPR_FLAG0;
    sprite_at(r, flag, team, vec2(x + w - 9.0f, y + 2.0f), 0.0f, false, false);
    sprite_at(r, id, flash ? SPRITE_PALETTE_FLASH : team, foot, 0.0f, false, false);
}

static void draw_bases(Render *r, const GameState *gs, i32 ox, i32 oy) {
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        if (gs->teams[t].has_base) {
            draw_base(r, gs, t, ox, oy);
        }
    }
}

static const Color COL_STEEL = {150, 156, 172, 255};
static const Color COL_STEEL_DARK = {84, 88, 104, 255};

// The 4x2 area of every pad: a steel plate striped in the team color, and a bracket at the corners of every cell.
static void draw_pads(Render *r, const GameState *gs, i32 ox, i32 oy) {
    for (i32 team = 0; team < MAX_TEAMS; team++) {
        if (!gs->level->pads[team].present) {
            continue;
        }
        const Rect pr = pad_rect(gs, team);
        const f32 x = pr.x - (f32)ox, y = pr.y - (f32)oy, bottom = y + pr.h;
        if (x + pr.w < -8.0f || x > (f32)LOGICAL_W + 8.0f || bottom < -8.0f || y > (f32)LOGICAL_H + 8.0f) {
            continue;
        }
        render_fill_rect(r, (Rect){x, y, pr.w, pr.h}, (Color){COL_PAD_PLATE.r, COL_PAD_PLATE.g, COL_PAD_PLATE.b, 22});
        render_fill_rect(r, (Rect){x - 2.0f, bottom, pr.w + 4.0f, 4.0f}, COL_STEEL_DARK);
        render_fill_rect(r, (Rect){x - 2.0f, bottom, pr.w + 4.0f, 1.0f}, COL_STEEL);
        const Color stripe = ui_team_color(team);
        for (f32 sx = x; sx < x + pr.w - 2.0f; sx += 6.0f) { // hazard stripes
            render_fill_rect(r, (Rect){sx + 1.0f, bottom + 1.0f, 3.0f, 2.0f}, stripe);
        }
        for (f32 sx = x; sx <= x + pr.w; sx += pr.w) { // end rivets
            render_fill_rect(r, (Rect){sx - 1.0f, bottom + 1.0f, 1.0f, 1.0f}, COL_STEEL);
        }
        const Color mark = {COL_PAD_PLATE.r, COL_PAD_PLATE.g, COL_PAD_PLATE.b, 170};
        for (i32 row = 0; row <= PAD_ROWS; row++) {
            for (i32 col = 0; col <= PAD_COLS; col++) {
                const f32 cx = x + (f32)(col * TILE_SIZE), cy = y + (f32)(row * TILE_SIZE);
                render_fill_rect(r, (Rect){cx - 2.0f, cy, 5.0f, 1.0f}, mark); // a small cross at every corner
                render_fill_rect(r, (Rect){cx, cy - 2.0f, 1.0f, 5.0f}, mark);
            }
        }
    }
}

// Vehicles are not in the game yet: what the pad makes is a tank model waiting on the ground.
static void draw_products(Render *r, const GameState *gs, f32 alpha, i32 ox, i32 oy) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type != ENT_PRODUCT) {
            continue;
        }
        const f32 x = roundf(lerpf(e->prev_pos.x, e->body.pos.x, alpha));
        const f32 y = roundf(lerpf(e->prev_pos.y, e->body.pos.y, alpha));
        const SpriteId id = e->as.product.kind == PRODUCT_HEAVY_TANK ? SPR_HEAVY_TANK : SPR_TANK;
        const Vec2 foot = vec2(x + e->body.size.x * 0.5f - (f32)ox, y + e->body.size.y - (f32)oy);
        sprite_at(r, id, e->as.product.team, foot, 0.0f, false, false);
    }
}

// A weapon on the ground: a small case with the weapon floating above it.
static void draw_loot_weapons(Render *r, const GameState *gs, f32 alpha, i32 ox, i32 oy) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type != ENT_WEAPON) {
            continue;
        }
        const f32 x = roundf(lerpf(e->prev_pos.x, e->body.pos.x, alpha));
        const f32 y = roundf(lerpf(e->prev_pos.y, e->body.pos.y, alpha));
        const f32 w = e->body.size.x, h = e->body.size.y;
        sprite_at(r, SPR_CASE, 0, vec2(x + w * 0.5f - (f32)ox, y + h - (f32)oy), 0.0f, false, false);
        const SpriteId ws = weapon_sprite(e->as.weapon.kind);
        if (ws != SPR_COUNT) {
            const f32 hover = roundf(sinf((f32)gs->tick * 0.1f + (f32)i) * 1.5f);
            sprite_at(r, ws, 0, vec2(x + w * 0.5f - (f32)ox, y + h - 9.0f + hover - (f32)oy), 0.0f, false, false);
        }
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
        const f32 wobble = d->hurt_ticks > 0 ? sinf((f32)d->hurt_ticks * 1.3f) * 6.0f : 0.0f; // rocks when hit
        const SpriteDraw sd = {.id = SPR_DUMMY,
                               .palette = flash ? SPRITE_PALETTE_FLASH : 0,
                               .pos = vec2(x + w * 0.5f - (f32)ox, y + h - (f32)oy),
                               .angle = wobble};
        render_sprite(r, &sd);
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
        const f32 x = lerpf(e->prev_pos.x, e->body.pos.x, alpha);
        const f32 y = lerpf(e->prev_pos.y, e->body.pos.y, alpha);
        const Vec2 c = vec2(x + e->body.size.x * 0.5f - (f32)ox, y + e->body.size.y * 0.5f - (f32)oy);
        const Vec2 v = e->body.vel;
        const f32 sp = sqrtf(v.x * v.x + v.y * v.y);
        const Vec2 dir = sp > 0.0f ? vec2_scale(v, 1.0f / sp) : vec2(1.0f, 0.0f);
        switch (weapon_def(e->as.projectile.weapon)->shot) {
        case SHOT_GRENADE:
            sprite_at(r, SPR_GRENADE, 0, c, (f32)((gs->tick * 23u + i * 40u) % 360u), false, false);
            break;
        case SHOT_ROCKET:
            sprite_at(r, SPR_ROCKET, 0, c, deg(dir), false, false);
            fill_circle(r, vec2_sub(c, vec2_scale(dir, 7.0f)), 1.5f, (Color){255, 200, 80, 255}); // exhaust
            break;
        default: // a short streak with a bright head
            render_line(r, vec2_sub(c, vec2_scale(dir, 5.0f)), c, (Color){255, 200, 90, 200});
            render_fill_rect(r, (Rect){roundf(c.x) - 1.0f, roundf(c.y) - 1.0f, 2.0f, 2.0f}, COL_BULLET);
            break;
        }
    }
}

static Color with_alpha(u8 r, u8 g, u8 b, f32 a) {
    return (Color){r, g, b, (u8)(clampf(a, 0.0f, 1.0f) * 255.0f)};
}

static void draw_ring(Render *r, Vec2 c, f32 radius, Color color) {
    const int n = 24;
    for (int k = 0; k < n; k++) {
        const f32 a0 = (f32)k / (f32)n * 6.2831853f, a1 = (f32)(k + 1) / (f32)n * 6.2831853f;
        render_line(r, vec2(c.x + cosf(a0) * radius, c.y + sinf(a0) * radius),
                    vec2(c.x + cosf(a1) * radius, c.y + sinf(a1) * radius), color);
    }
}

static void draw_particle(Render *r, const Particle *p, i32 ox, i32 oy) {
    const f32 t = particle_progress(p);
    const Vec2 at = vec2(p->pos.x - (f32)ox, p->pos.y - (f32)oy);
    if (at.x < -40.0f || at.x > (f32)LOGICAL_W + 40.0f || at.y < -40.0f || at.y > (f32)LOGICAL_H + 40.0f) {
        return;
    }
    switch ((ParticleKind)p->kind) {
    case PART_DOT:
        render_fill_rect(r, (Rect){roundf(at.x), roundf(at.y), roundf(p->size), roundf(p->size)},
                         with_alpha(p->r, p->g, p->b, 1.0f - t * t));
        break;
    case PART_SMOKE:
        fill_circle(r, at, p->size, with_alpha(p->r, p->g, p->b, 0.75f * (1.0f - t)));
        break;
    case PART_SPARK:
        render_line(r, at, vec2_sub(at, vec2_scale(p->vel, 0.025f)), with_alpha(p->r, p->g, p->b, 1.0f - t));
        break;
    case PART_CHUNK: { // tumbling: wide then tall
        const bool wide = ((i32)(fabsf(p->angle) / 90.0f)) % 2 == 0;
        const f32 w = wide ? p->size : MAX(1.0f, p->size * 0.6f), h = wide ? MAX(1.0f, p->size * 0.6f) : p->size;
        render_fill_rect(r, (Rect){roundf(at.x - w * 0.5f), roundf(at.y - h * 0.5f), roundf(w), roundf(h)},
                         with_alpha(p->r, p->g, p->b, t < 0.8f ? 1.0f : (1.0f - t) * 5.0f));
        break;
    }
    case PART_STAR: {
        const SpriteDraw d = {.id = SPR_STAR, .pos = at, .angle = p->angle, .alpha = (u8)(255.0f * (1.0f - t * t))};
        render_sprite(r, &d);
        break;
    }
    case PART_RING:
        draw_ring(r, at, p->size, with_alpha(p->r, p->g, p->b, 1.0f - t));
        break;
    case PART_ICON: {
        const SpriteDraw d = {.id = (SpriteId)p->variant,
                              .pos = at,
                              .pivot_center = true,
                              .alpha = (u8)MAX(1.0f, 255.0f * (t < 0.6f ? 1.0f : (1.0f - t) / 0.4f))};
        render_sprite(r, &d);
        break;
    }
    case PART_CORPSE: {
        const SpriteDraw d = {.id = sprite_body(p->variant, POSE_FALL),
                              .palette = p->team,
                              .pos = vec2(at.x, at.y + (f32)SPRITE_BODY_H * 0.5f),
                              .angle = p->angle,
                              .flip_x = p->facing < 0,
                              .pivot_center = true,
                              .alpha = (u8)(255.0f * (t < 0.6f ? 1.0f : (1.0f - t) / 0.4f))};
        render_sprite(r, &d);
        break;
    }
    }
}

static void draw_fx(Render *r, const FxList *fx, i32 ox, i32 oy) {
    if (!fx) {
        return;
    }
    for (u32 i = 0; i < fx->part_count; i++) {
        draw_particle(r, &fx->parts[i], ox, oy);
    }
    for (u32 i = 0; i < fx->count; i++) {
        const Fx *f = &fx->items[i];
        const f32 t = fx_progress(f);
        const Vec2 c = vec2(f->pos.x - (f32)ox, f->pos.y - (f32)oy);
        switch (f->kind) {
        case FX_EXPLOSION:
            fill_circle(r, c, f->radius * (0.35f + 0.65f * t), (Color){255, (u8)lerpf(150.0f, 40.0f, t), 30, (u8)lerpf(255.0f, 60.0f, t)});
            fill_circle(r, c, f->radius * (0.2f + 0.45f * t) * (1.0f - t * 0.5f), (Color){255, 244, (u8)lerpf(190.0f, 60.0f, t), (u8)lerpf(255.0f, 0.0f, t)});
            break;
        case FX_SPARK:
            render_fill_rect(r, (Rect){c.x - f->radius * (1.0f - t), c.y - 1.0f, f->radius * 2.0f * (1.0f - t), 2.0f}, COL_BULLET);
            render_fill_rect(r, (Rect){c.x - 1.0f, c.y - f->radius * (1.0f - t), 2.0f, f->radius * 2.0f * (1.0f - t)}, COL_BULLET);
            break;
        case FX_MUZZLE:
            fill_circle(r, c, f->radius, COL_BULLET);
            fill_circle(r, c, f->radius * 0.5f, COL_FLASH);
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

    const SpriteId ws = weapon_sprite(c->weapon);
    const SpriteDraw icon = {.id = ws == SPR_COUNT ? SPR_PUNCH : ws,
                             .palette = team,
                             .pos = vec2(16.0f, 25.5f),
                             .pivot_center = true,
                             .scale_x = ws == SPR_ROCKET_LAUNCHER ? 0.75f : 1.0f,
                             .scale_y = ws == SPR_ROCKET_LAUNCHER ? 0.75f : 1.0f};
    render_sprite(r, &icon);
    if (c->weapon == WEAPON_FISTS) {
        ui_text_shadow(r, 26.0f, 22.0f, ui_weapon_label(c->weapon), COL_TEXT, 1, ALIGN_LEFT);
    } else {
        snprintf(line, sizeof(line), "%s", ui_weapon_label(c->weapon));
        ui_text_shadow(r, 26.0f, 22.0f, line, COL_TEXT, 1, ALIGN_LEFT);
        snprintf(line, sizeof(line), "%d", c->ammo);
        ui_text_shadow(r, 134.0f, 22.0f, line, c->ammo * 4 <= weapon_def(c->weapon)->max_ammo ? COL_HP_LOW : COL_TEXT, 1,
                       ALIGN_RIGHT);
    }

    const Order order = gs->teams[team].order;
    ui_text_shadow(r, 9.0f, 35.0f, "ORDRE", COL_TEXT_DIM, 1, ALIGN_LEFT);
    ui_text_shadow(r, 44.0f, 35.0f, ui_order_label(order), ORDER_COLORS[order], 1, ALIGN_LEFT);

    // One figure per living soldier of the team, and their number.
    i32 n = 0;
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        if (gs->soldiers[i].alive && gs->soldiers[i].team == team) {
            if (n < 12) { // a small figure per soldier, the controlled one underlined
                const f32 px = 13.0f + (f32)(n * 9);
                const SpriteDraw d = {.id = sprite_body((i32)gs->soldiers[i].variant, POSE_IDLE0),
                                      .palette = team,
                                      .pos = vec2(px, 58.0f),
                                      .scale_x = 0.5f,
                                      .scale_y = 0.5f};
                render_sprite(r, &d);
                if (i == gs->controlled) {
                    render_fill_rect(r, (Rect){px - 4.0f, 59.0f, 8.0f, 1.0f}, COL_MARKER);
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

// What a construction gives, standing on `foot`: the weapon, the soldier (small) or the tank.
static void draw_product_icon(Render *r, PadProduct product, i32 team, Vec2 foot) {
    WeaponKind weapon;
    SoldierVariant variant;
    if (weapon_from_product(product, &weapon)) {
        const SpriteId ws = weapon_sprite(weapon);
        const SpriteDraw d = {.id = ws, .palette = team, .pos = vec2(foot.x, foot.y - 6.0f), .pivot_center = true};
        render_sprite(r, &d);
    } else if (soldier_from_product(product, &variant)) {
        const SpriteDraw d = {.id = sprite_body((i32)variant, POSE_IDLE0), .palette = team, .pos = foot,
                              .scale_x = 0.625f, .scale_y = 0.625f};
        render_sprite(r, &d);
    } else {
        sprite_at(r, product == PRODUCT_HEAVY_TANK ? SPR_HEAVY_TANK : SPR_TANK, team, foot, 0.0f, false, false);
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
    draw_product_icon(r, o->product, m->team, vec2(box.x + box.w - 12.0f, box.y + box.h - 3.0f));
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
    const Vec2 shake = fx ? fx_shake_offset(fx) : vec2(0.0f, 0.0f);
    const i32 ox = (i32)roundf(cp.x + shake.x);
    const i32 oy = (i32)roundf(cp.y + shake.y);

    render_begin(r, (Color){24, 28, 44, 255});
    draw_background(r, ox, oy);
    draw_tiles(r, gs->level, ox, oy, gs->tick);
    draw_bases(r, gs, ox, oy);
    draw_pads(r, gs, ox, oy);
    draw_crates(r, gs, fx, alpha, ox, oy, flags);
    draw_crate_cues(r, gs, ox, oy);
    draw_products(r, gs, alpha, ox, oy);
    draw_loot_weapons(r, gs, alpha, ox, oy);
    draw_dummies(r, gs, alpha, ox, oy);
    draw_soldiers(r, gs, fx, alpha, ox, oy);
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
