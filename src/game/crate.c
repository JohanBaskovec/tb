#include "game/crate.h"

#include "core/log.h"
#include "game/pad.h"
#include "game/tuning.h"
#include "game/weapon.h"

#include <math.h>

#define CRATE_SIZE TUNING_CRATE_SIZE

// ---- Grid helpers ----------------------------------------------------------------------------

// Is there something under the cell that a crate can rest on?
static bool cell_supported(const Level *lv, i32 cx, i32 cy) {
    return level_is_solid(lv, cx, cy + 1) || level_is_one_way(lv, cx, cy + 1);
}

static bool rect_hits_solid(const Level *lv, f32 x, f32 y, f32 w, f32 h) {
    const i32 tx0 = level_px_to_tile(x + 0.001f), tx1 = level_px_to_tile(x + w - 0.001f);
    const i32 ty0 = level_px_to_tile(y + 0.001f), ty1 = level_px_to_tile(y + h - 0.001f);
    for (i32 ty = ty0; ty <= ty1; ty++) {
        for (i32 tx = tx0; tx <= tx1; tx++) {
            if (level_is_solid(lv, tx, ty)) {
                return true;
            }
        }
    }
    return false;
}

static EntityId slot_id(const GameState *gs, u16 slot) {
    return (EntityId){slot, gs->entities.slots[slot].generation};
}

static bool is_live_crate(const Entity *e) {
    return e->type == ENT_CRATE && !e->pending_destroy;
}

u32 crate_count(const GameState *gs) {
    u32 n = 0;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        n += is_live_crate(&gs->entities.slots[i]) ? 1u : 0u;
    }
    return n;
}

// ---- Create / destroy ------------------------------------------------------------------------

EntityId crate_spawn(GameState *gs, CrateKind kind, Vec2 pos, CrateState state, i32 origin_zone) {
    EntityId id = pool_create(&gs->entities, ENT_CRATE);
    if (!entity_id_valid(id)) {
        LOG_WARN("entity pool is full: crate not spawned");
        return ENTITY_NONE;
    }
    Entity *e = pool_get(&gs->entities, id);
    e->body.size = vec2(CRATE_SIZE, CRATE_SIZE);
    e->body.pos = pos;
    e->prev_pos = pos;
    e->as.crate = (Crate){.kind = kind, .state = state, .origin_zone = origin_zone};
    if (state == CRATE_RESTING) {
        const i32 cx = level_px_to_tile(pos.x + 0.5f), cy = level_px_to_tile(pos.y + 0.5f);
        ASSERT(pos.x == (f32)(cx * TILE_SIZE) && pos.y == (f32)(cy * TILE_SIZE));
        ASSERT(!level_is_solid(gs->level, cx, cy));
        e->as.crate.cell_x = cx;
        e->as.crate.cell_y = cy;
        level_set_dyn_solid(gs->level, cx, cy, true);
    }
    gs->crate_stats.spawned++;
    events_push(&gs->events, EV_CRATE_SPAWNED, pos, (i32)kind, origin_zone);
    return id;
}

typedef enum Removal { REMOVAL_DESTROYED, REMOVAL_CONSUMED, REMOVAL_SALVAGED } Removal;

// `REMOVAL_SALVAGED` announces itself (crate_salvage knows the reward).
static void crate_remove(GameState *gs, EntityId id, Removal how) {
    Entity *e = pool_get(&gs->entities, id);
    if (!e || e->pending_destroy || e->type != ENT_CRATE) {
        return;
    }
    Crate *c = &e->as.crate;
    if (c->state == CRATE_RESTING) {
        level_set_dyn_solid(gs->level, c->cell_x, c->cell_y, false);
    }
    if (c->state == CRATE_CARRIED) {
        for (i32 i = 0; i < MAX_SOLDIERS; i++) {
            if (gs->soldiers[i].alive && entity_id_eq(gs->soldiers[i].c.carrying, id)) {
                gs->soldiers[i].c.carrying = ENTITY_NONE;
            }
        }
    }
    pool_mark_destroy(&gs->entities, id);
    if (how == REMOVAL_SALVAGED) {
        gs->crate_stats.salvaged++;
        return;
    }
    if (how == REMOVAL_CONSUMED) {
        gs->crate_stats.consumed++;
    } else {
        gs->crate_stats.destroyed++;
    }
    events_push(&gs->events, how == REMOVAL_CONSUMED ? EV_CRATE_CONSUMED : EV_CRATE_DESTROYED, e->body.pos,
                (i32)c->kind, 0);
}

void crate_destroy(GameState *gs, EntityId id) {
    crate_remove(gs, id, REMOVAL_DESTROYED);
}

void crate_consume(GameState *gs, EntityId id) {
    crate_remove(gs, id, REMOVAL_CONSUMED);
}

// ---- Crates present at the start ---------------------------------------------------------------

static CrateKind zone_kind(GameState *gs, const DropZoneConfig *cfg) {
    if (cfg->color == DROP_BLUE) {
        return CRATE_BLUE;
    }
    if (cfg->color == DROP_MIXED) {
        return (i32)rng_below(&gs->rng, 100) < cfg->blue_percent ? CRATE_BLUE : CRATE_RED;
    }
    return CRATE_RED;
}

// Is the cell part of a pad or of a base (where a crate would be in the way)?
static bool reserved_cell(const GameState *gs, i32 cx, i32 cy) {
    const Rect cell = {(f32)(cx * TILE_SIZE), (f32)(cy * TILE_SIZE), CRATE_SIZE, CRATE_SIZE};
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        const PadSite *p = &gs->level->pads[t];
        if (p->present && cx >= p->cell_x && cx < p->cell_x + PAD_COLS && cy <= p->cell_y && cy > p->cell_y - PAD_ROWS) {
            return true;
        }
        if (gs->level->has_spawn[t]) {
            const Vec2 s = gs->level->spawns[t];
            const Rect base = {s.x - TUNING_BASE_W * 0.5f, s.y - TUNING_BASE_H, TUNING_BASE_W, TUNING_BASE_H};
            if (rect_overlap(cell, base)) {
                return true;
            }
        }
    }
    return false;
}

// The free cell a crate dropped in column `cx` from row `cy0` would rest on, or false (water, ladder, wall...).
static bool landing_cell(const GameState *gs, i32 cx, i32 cy0, i32 *out_cy) {
    const Level *lv = gs->level;
    if (cx < 0 || cx >= lv->width) {
        return false;
    }
    for (i32 cy = cy0; cy < lv->height; cy++) {
        const TileType t = level_tile(lv, cx, cy);
        if (level_is_solid(lv, cx, cy) || t == TILE_HAZARD || t == TILE_LADDER) {
            return false;
        }
        if (cell_supported(lv, cx, cy)) {
            if (reserved_cell(gs, cx, cy)) {
                return false;
            }
            *out_cy = cy;
            return true;
        }
    }
    return false;
}

void crates_place_start(GameState *gs) {
    const Level *lv = gs->level;
    for (i32 z = 0; z < lv->drop_zone_count; z++) {
        const i32 zx = level_px_to_tile(lv->drop_zones[z].x), zy = level_px_to_tile(lv->drop_zones[z].y + 0.5f);
        i32 placed = 0;
        // Under the zone, then alternately one column further right and left.
        for (i32 k = 0; k < 12 && placed < lv->drop_start[z]; k++) {
            const i32 cx = zx + ((k % 2 == 0) ? k / 2 : -(k + 1) / 2);
            i32 cy;
            if (landing_cell(gs, cx, zy, &cy)) {
                crate_spawn(gs, zone_kind(gs, &lv->drop_cfg[z]), vec2((f32)(cx * TILE_SIZE), (f32)(cy * TILE_SIZE)),
                            CRATE_RESTING, z);
                placed++;
            }
        }
    }
}

// ---- Drop zones ------------------------------------------------------------------------------

static u32 zone_crate_count(const GameState *gs, i32 zone) {
    u32 n = 0;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        n += (is_live_crate(e) && e->as.crate.origin_zone == zone) ? 1u : 0u;
    }
    return n;
}

// A crate that would land on another one goes to a neighbouring column whose ground is free, if there is one:
// crates spread out instead of piling up into a tower nobody can reach. Returns the column shift (-1, 0, +1).
static i32 spread_column(GameState *gs, i32 zone) {
    const Level *lv = gs->level;
    const i32 zx = level_px_to_tile(lv->drop_zones[zone].x), zy = level_px_to_tile(lv->drop_zones[zone].y + 0.5f);
    i32 cy;
    if (!landing_cell(gs, zx, zy, &cy)) {
        return 0;
    }
    bool crowded = level_dyn_solid(lv, zx, cy + 1);
    for (u16 i = 1; i <= MAX_ENTITIES && !crowded; i++) { // a crate still on its way down there counts too
        const Entity *e = &gs->entities.slots[i];
        crowded = is_live_crate(e) && (e->as.crate.state == CRATE_DESCENDING || e->as.crate.state == CRATE_FALLING) &&
                  level_px_to_tile(e->body.pos.x + CRATE_SIZE * 0.5f) == zx;
    }
    if (!crowded) {
        return 0;
    }
    const i32 first = rng_below(&gs->rng, 2) ? 1 : -1;
    for (i32 k = 0; k < 2; k++) {
        const i32 shift = k == 0 ? first : -first;
        if (landing_cell(gs, zx + shift, zy, &cy) && !level_dyn_solid(lv, zx + shift, cy + 1)) {
            return shift;
        }
    }
    return 0;
}

static void spawn_from_zones(GameState *gs) {
    Level *lv = gs->level;
    for (i32 z = 0; z < lv->drop_zone_count; z++) {
        const DropZoneConfig *cfg = &lv->drop_cfg[z];
        if (zone_crate_count(gs, z) >= (u32)cfg->cap) {
            continue; // at the cap: the timer waits until a crate of this zone disappears
        }
        if (gs->drop_timers[z] > 0) {
            gs->drop_timers[z]--;
            continue;
        }
        gs->drop_timers[z] = (u32)cfg->interval_s * TICK_HZ;

        const Vec2 zone = lv->drop_zones[z];
        const i32 shift = spread_column(gs, z);
        const Vec2 pos = vec2(zone.x - CRATE_SIZE * 0.5f + (f32)(shift * TILE_SIZE), zone.y);
        if (rect_hits_solid(lv, pos.x, pos.y, CRATE_SIZE, CRATE_SIZE)) {
            continue; // the pile reached the sky: skip this drop
        }
        const CrateKind kind = zone_kind(gs, cfg);
        crate_spawn(gs, kind, pos, CRATE_DESCENDING, z);
    }
}

// ---- Falling and settling --------------------------------------------------------------------

// A landed crate snaps to a grid cell: the column it overlaps most (and that is supported), on the
// row it landed on. It waits while a soldier stands in that cell.
static void try_settle(GameState *gs, EntityId id, Entity *e) {
    Level *lv = gs->level;
    const Body *b = &e->body;
    const i32 row = level_px_to_tile(b->pos.y + b->size.y - 0.5f);
    const f32 x = b->pos.x;
    const i32 c0 = level_px_to_tile(x + 0.001f);
    const i32 c1 = level_px_to_tile(x + CRATE_SIZE - 0.001f);
    i32 col = c0;
    if (c1 != c0) {
        const f32 overlap0 = (f32)((c0 + 1) * TILE_SIZE) - x;
        const f32 overlap1 = x + CRATE_SIZE - (f32)(c1 * TILE_SIZE);
        const bool s0 = cell_supported(lv, c0, row);
        const bool s1 = cell_supported(lv, c1, row);
        if (s0 != s1) {
            col = s0 ? c0 : c1;
        } else {
            col = overlap0 >= overlap1 ? c0 : c1;
        }
    }
    i32 r = row;
    while (r >= row - 3 && level_is_solid(lv, col, r)) {
        r--;
    }
    if (level_is_solid(lv, col, r)) {
        crate_destroy(gs, id); // no room at all
        return;
    }
    const Rect cell = {(f32)(col * TILE_SIZE), (f32)(r * TILE_SIZE), CRATE_SIZE, CRATE_SIZE};
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        if (gs->soldiers[i].alive && rect_overlap(cell, body_rect(&gs->soldiers[i].c.body))) {
            e->body.vel = vec2(0.0f, 0.0f); // wait for the soldier to move away
            return;
        }
    }
    Crate *c = &e->as.crate;
    e->body.pos = vec2(cell.x, cell.y);
    e->body.vel = vec2(0.0f, 0.0f);
    c->state = CRATE_RESTING;
    c->cell_x = col;
    c->cell_y = r;
    level_set_dyn_solid(lv, col, r, true);
    gs->crate_stats.settled++;
    events_push(&gs->events, EV_CRATE_SETTLED, e->body.pos, (i32)c->kind, 0);
}

static void crate_step(GameState *gs, u16 slot) {
    Level *lv = gs->level;
    Entity *e = &gs->entities.slots[slot];
    Crate *c = &e->as.crate;
    const EntityId id = slot_id(gs, slot);
    e->prev_pos = e->body.pos;

    switch (c->state) {
    case CRATE_RESTING:
        if (!cell_supported(lv, c->cell_x, c->cell_y)) {
            level_set_dyn_solid(lv, c->cell_x, c->cell_y, false);
            c->state = CRATE_FALLING;
            e->body.vel = vec2(0.0f, 0.0f);
        }
        return;
    case CRATE_CARRIED:
        return;
    case CRATE_DESCENDING:
        e->body.vel = vec2(0.0f, TUNING_CRATE_DESCENT_SPEED);
        break;
    case CRATE_FALLING:
        e->body.vel.y = MIN(e->body.vel.y + TUNING_GRAVITY * TICK_DT, TUNING_MAX_FALL_SPEED);
        break;
    }

    body_move(&e->body, lv, TICK_DT, false);
    if (body_overlaps_tile(&e->body, lv, TILE_HAZARD)) {
        crate_destroy(gs, id); // lost in the water or in the void
        return;
    }
    if (e->body.on_ground) {
        try_settle(gs, id, e);
    }
}

void crates_update(GameState *gs) {
    spawn_from_zones(gs);

    // Bottom-most crates first (ties by slot): a stack settles from the bottom up in a single tick.
    u16 order[MAX_ENTITIES];
    u32 n = 0;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (!is_live_crate(e) || e->as.crate.state == CRATE_CARRIED) {
            continue;
        }
        u32 j = n;
        while (j > 0 && gs->entities.slots[order[j - 1]].body.pos.y < e->body.pos.y) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = i;
        n++;
    }
    for (u32 k = 0; k < n; k++) {
        if (!gs->entities.slots[order[k]].pending_destroy) {
            crate_step(gs, order[k]);
        }
    }
}

// ---- Carrying --------------------------------------------------------------------------------

static void carry_set_position(const Companion *p, Entity *e) {
    const Body *b = &p->body;
    e->prev_pos = e->body.pos;
    e->body.pos = vec2(b->pos.x + b->size.x * 0.5f - CRATE_SIZE * 0.5f, b->pos.y - CRATE_SIZE);
}

void crate_carried_follow(GameState *gs) {
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        Companion *p = &gs->soldiers[i].c;
        if (!gs->soldiers[i].alive || !entity_id_valid(p->carrying)) {
            continue;
        }
        Entity *e = pool_get(&gs->entities, p->carrying);
        if (!e || e->pending_destroy) {
            p->carrying = ENTITY_NONE;
            continue;
        }
        carry_set_position(p, e);
    }
}

// The reachable resting crate closest to the soldier that has nothing on top of it.
static EntityId find_pickup(const GameState *gs, const Companion *p) {
    const Body *b = &p->body;
    const f32 reach = TUNING_PICKUP_REACH;
    const Rect zone = p->facing > 0
                          ? (Rect){b->pos.x + b->size.x - 2.0f, b->pos.y, reach + 2.0f, b->size.y - 2.0f}
                          : (Rect){b->pos.x - reach, b->pos.y, reach + 2.0f, b->size.y - 2.0f};
    const f32 cx = b->pos.x + b->size.x * 0.5f;

    EntityId best = ENTITY_NONE;
    f32 best_dist = 0.0f;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (!is_live_crate(e) || e->as.crate.state != CRATE_RESTING) {
            continue;
        }
        if (!rect_overlap(body_rect(&e->body), zone)) {
            continue;
        }
        if (level_dyn_solid(gs->level, e->as.crate.cell_x, e->as.crate.cell_y - 1)) {
            continue; // something on top: only the top of a stack can be taken
        }
        const f32 dist = fabsf(e->body.pos.x + CRATE_SIZE * 0.5f - cx);
        if (!entity_id_valid(best) || dist < best_dist) {
            best = slot_id(gs, i);
            best_dist = dist;
        }
    }
    return best;
}

EntityId crate_pickup_target(const GameState *gs, const Companion *p) {
    return entity_id_valid(p->carrying) || p->on_ladder ? ENTITY_NONE : find_pickup(gs, p);
}

static void try_pick_up(GameState *gs, Companion *p) {
    EntityId id = find_pickup(gs, p);
    if (!entity_id_valid(id)) {
        return;
    }
    Entity *e = pool_get(&gs->entities, id);
    Crate *c = &e->as.crate;
    level_set_dyn_solid(gs->level, c->cell_x, c->cell_y, false);
    c->state = CRATE_CARRIED;
    e->body.vel = vec2(0.0f, 0.0f);
    e->prev_pos = e->body.pos;
    p->carrying = id;
    gs->crate_stats.picked_up++;
    events_push(&gs->events, EV_CRATE_PICKED_UP, e->body.pos, (i32)c->kind, 0);
}

// The tile in the first column entirely in front of the body, on the feet row, or up to TUNING_PLACE_MAX_RAISE
// rows higher when that cell is taken (to build stacks).
static bool find_place(const GameState *gs, const Companion *p, i32 *out_col, i32 *out_row) {
    const Body *b = &p->body;
    const i32 col = p->facing > 0 ? (i32)ceilf((b->pos.x + b->size.x) / (f32)TILE_SIZE)
                                  : level_px_to_tile(b->pos.x) - 1;
    const i32 row0 = level_px_to_tile(b->pos.y + b->size.y - 0.5f);
    for (i32 raise = 0; raise <= TUNING_PLACE_MAX_RAISE; raise++) {
        const i32 row = row0 - raise;
        if (!level_is_solid(gs->level, col, row)) {
            *out_col = col;
            *out_row = row;
            return true;
        }
    }
    return false;
}

bool crate_place_target(const GameState *gs, const Companion *p, i32 *col, i32 *row) {
    return entity_id_valid(p->carrying) && find_place(gs, p, col, row);
}

static bool try_place(GameState *gs, Companion *p) {
    i32 col, row;
    if (!find_place(gs, p, &col, &row)) {
        return false;
    }
    Entity *e = pool_get(&gs->entities, p->carrying);
    e->body.pos = vec2((f32)(col * TILE_SIZE), (f32)(row * TILE_SIZE));
    e->prev_pos = e->body.pos;
    e->body.vel = vec2(0.0f, 0.0f);
    e->as.crate.state = CRATE_FALLING;
    p->carrying = ENTITY_NONE;
    gs->crate_stats.placed++;
    events_push(&gs->events, EV_CRATE_PLACED, e->body.pos, (i32)e->as.crate.kind, 0);
    return true;
}

// Lobs the crate in the facing direction from above the head. Blocked by a low ceiling.
static bool try_throw(GameState *gs, Companion *p) {
    const Body *b = &p->body;
    const f32 x = b->pos.x + b->size.x * 0.5f - CRATE_SIZE * 0.5f;
    const f32 y = b->pos.y - CRATE_SIZE;
    if (rect_hits_solid(gs->level, x, y, CRATE_SIZE, CRATE_SIZE)) {
        return false;
    }
    Entity *e = pool_get(&gs->entities, p->carrying);
    e->body.pos = vec2(x, y);
    e->prev_pos = e->body.pos;
    e->body.vel = vec2((f32)p->facing * TUNING_THROW_VX, -TUNING_THROW_VY);
    e->as.crate.state = CRATE_FALLING;
    p->carrying = ENTITY_NONE;
    p->fire_cooldown = MAX(p->fire_cooldown, SECONDS_TO_TICKS(TUNING_THROW_RECOVERY_S)); // the key may still be held
    gs->crate_stats.thrown++;
    events_push(&gs->events, EV_CRATE_THROWN, e->body.pos, (i32)e->as.crate.kind, 0);
    return true;
}

// Breaks a crate and gives the soldier hearts or ammunition, at random. Whatever would be wasted (hearts at full health,
// ammunition for fists or a full weapon) is swapped for the other reward when that one is useful.
static void salvage(GameState *gs, Companion *p, EntityId id) {
    const Entity *e = pool_get_const(&gs->entities, id);
    if (!e) {
        return;
    }
    const WeaponDef *def = weapon_def(p->weapon);
    const bool can_heal = p->hp > 0 && p->hp < p->max_hp;
    const bool can_reload = def->max_ammo > 0 && p->ammo < def->max_ammo;
    bool hearts = (i32)rng_below(&gs->rng, 100) < TUNING_SALVAGE_HEART_PERCENT;
    if (hearts && !can_heal && can_reload) {
        hearts = false;
    } else if (!hearts && !can_reload && can_heal) {
        hearts = true;
    }
    SalvageReward reward = SALVAGE_NOTHING;
    i32 amount = 0;
    if (hearts && can_heal) {
        reward = SALVAGE_HEARTS;
        amount = MIN(TUNING_SALVAGE_HEAL, p->max_hp - p->hp);
        p->hp += amount;
    } else if (!hearts && can_reload) {
        reward = SALVAGE_AMMO;
        amount = MIN(MAX(1, def->max_ammo * TUNING_SALVAGE_AMMO_PERCENT / 100), def->max_ammo - p->ammo);
        p->ammo += amount;
    }
    const Vec2 pos = e->body.pos;
    crate_remove(gs, id, REMOVAL_SALVAGED);
    events_push(&gs->events, EV_CRATE_SALVAGED, pos, (i32)reward, amount);
}

void crate_carry_update(GameState *gs, Companion *p, const InputFrame *in) {
    const bool grab = input_pressed(in, ACT_GRAB);
    const bool fire = input_pressed(in, ACT_FIRE);
    const bool destroy = input_pressed(in, ACT_DESTROY);

    if (entity_id_valid(p->carrying)) {
        const Entity *held = pool_get_const(&gs->entities, p->carrying);
        if (!held || held->pending_destroy) {
            p->carrying = ENTITY_NONE;
            return;
        }
        if (destroy) {
            salvage(gs, p, p->carrying);
        } else if (fire) {
            try_throw(gs, p);
        } else if (grab) {
            try_place(gs, p);
        }
    } else if (grab && !p->on_ladder) {
        try_pick_up(gs, p);
    } else if (destroy && !p->on_ladder) {
        salvage(gs, p, find_pickup(gs, p));
    }
}

void crate_release_carried(GameState *gs, Companion *p) {
    Entity *e = pool_get(&gs->entities, p->carrying);
    p->carrying = ENTITY_NONE;
    if (e && !e->pending_destroy && e->as.crate.state == CRATE_CARRIED) {
        e->as.crate.state = CRATE_FALLING;
        e->body.vel = vec2(0.0f, 0.0f);
    }
}
