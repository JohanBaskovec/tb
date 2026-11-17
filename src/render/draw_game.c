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
    render_fill_rect(r, rect_at(ox, oy, x, y, w, h), COL_PLAYER);
    render_fill_rect(r, rect_at(ox, oy, x, y + h - 4.0f, w, 4.0f), COL_PLAYER_DARK);
    const f32 eye_x = c->facing > 0 ? x + w - 5.0f : x + 3.0f;
    render_fill_rect(r, rect_at(ox, oy, eye_x, y + 4.0f, 2.0f, 3.0f), COL_EYE);
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

void draw_game(Render *r, const GameState *gs, const Camera *cam, f32 alpha, u32 flags) {
    const Vec2 cp = camera_interpolated(cam, alpha);
    const i32 ox = (i32)roundf(cp.x);
    const i32 oy = (i32)roundf(cp.y);

    render_begin(r, (Color){24, 28, 44, 255});
    draw_background(r);
    draw_tiles(r, gs->level, ox, oy);
    draw_crates(r, gs, alpha, ox, oy, flags);
    draw_player(r, &gs->player, alpha, ox, oy);
    if (flags & DRAW_DEBUG) {
        draw_debug(r, gs, alpha, ox, oy);
    }
}
