#include "game/crate.h"

#include "core/log.h"
#include "game/tuning.h"

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

static void crate_remove(GameState *gs, EntityId id, bool consumed) {
    Entity *e = pool_get(&gs->entities, id);
    if (!e || e->pending_destroy || e->type != ENT_CRATE) {
        return;
    }
    Crate *c = &e->as.crate;
    if (c->state == CRATE_RESTING) {
        level_set_dyn_solid(gs->level, c->cell_x, c->cell_y, false);
    }
    if (c->state == CRATE_CARRIED && entity_id_eq(gs->player.carrying, id)) {
        gs->player.carrying = ENTITY_NONE;
    }
    pool_mark_destroy(&gs->entities, id);
    if (consumed) {
        gs->crate_stats.consumed++;
    } else {
        gs->crate_stats.destroyed++;
    }
    events_push(&gs->events, consumed ? EV_CRATE_CONSUMED : EV_CRATE_DESTROYED, e->body.pos,
                (i32)c->kind, 0);
}

void crate_destroy(GameState *gs, EntityId id) {
    crate_remove(gs, id, false);
}

void crate_consume(GameState *gs, EntityId id) {
    crate_remove(gs, id, true);
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
        const Vec2 pos = vec2(zone.x - CRATE_SIZE * 0.5f, zone.y);
        if (rect_hits_solid(lv, pos.x, pos.y, CRATE_SIZE, CRATE_SIZE)) {
            continue; // the pile reached the sky: skip this drop
        }
        CrateKind kind = CRATE_RED;
        if (cfg->color == DROP_BLUE) {
            kind = CRATE_BLUE;
        } else if (cfg->color == DROP_MIXED) {
            kind = (i32)rng_below(&gs->rng, 100) < cfg->blue_percent ? CRATE_BLUE : CRATE_RED;
        }
        crate_spawn(gs, kind, pos, CRATE_DESCENDING, z);
    }
}

// ---- Falling and settling --------------------------------------------------------------------

// A landed crate snaps to a grid cell: the column it overlaps most (and that is supported), on the
// row it landed on. It waits while the player stands in that cell.
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
    if (rect_overlap(cell, body_rect(&gs->player.body))) {
        e->body.vel = vec2(0.0f, 0.0f); // wait for the player to move away
        return;
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
    Companion *p = &gs->player;
    if (!entity_id_valid(p->carrying)) {
        return;
    }
    Entity *e = pool_get(&gs->entities, p->carrying);
    if (!e || e->pending_destroy) {
        p->carrying = ENTITY_NONE;
        return;
    }
    carry_set_position(p, e);
}

// The reachable resting crate closest to the player that has nothing on top of it.
static EntityId find_pickup(const GameState *gs) {
    const Companion *p = &gs->player;
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

static void try_pick_up(GameState *gs) {
    EntityId id = find_pickup(gs);
    if (!entity_id_valid(id)) {
        return;
    }
    Entity *e = pool_get(&gs->entities, id);
    Crate *c = &e->as.crate;
    level_set_dyn_solid(gs->level, c->cell_x, c->cell_y, false);
    c->state = CRATE_CARRIED;
    e->body.vel = vec2(0.0f, 0.0f);
    e->prev_pos = e->body.pos;
    gs->player.carrying = id;
    gs->crate_stats.picked_up++;
    events_push(&gs->events, EV_CRATE_PICKED_UP, e->body.pos, (i32)c->kind, 0);
}

// Puts the crate in the first column entirely in front of the body, on the feet row, or up to
// TUNING_PLACE_MAX_RAISE rows higher when that cell is taken (to build stacks).
static bool try_place(GameState *gs) {
    Companion *p = &gs->player;
    const Body *b = &p->body;
    const i32 col = p->facing > 0 ? (i32)ceilf((b->pos.x + b->size.x) / (f32)TILE_SIZE)
                                  : level_px_to_tile(b->pos.x) - 1;
    const i32 row0 = level_px_to_tile(b->pos.y + b->size.y - 0.5f);
    for (i32 raise = 0; raise <= TUNING_PLACE_MAX_RAISE; raise++) {
        const i32 row = row0 - raise;
        if (level_is_solid(gs->level, col, row)) {
            continue;
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
    return false;
}

// Lobs the crate in the facing direction from above the head. Blocked by a low ceiling.
static bool try_throw(GameState *gs) {
    Companion *p = &gs->player;
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
    gs->crate_stats.thrown++;
    events_push(&gs->events, EV_CRATE_THROWN, e->body.pos, (i32)e->as.crate.kind, 0);
    return true;
}

void crate_carry_update(GameState *gs, const InputFrame *in) {
    Companion *p = &gs->player;
    const bool grab = input_pressed(in, ACT_GRAB);
    const bool fire = input_pressed(in, ACT_FIRE);

    if (entity_id_valid(p->carrying)) {
        const Entity *held = pool_get_const(&gs->entities, p->carrying);
        if (!held || held->pending_destroy) {
            p->carrying = ENTITY_NONE;
            return;
        }
        if (fire) {
            try_throw(gs);
        } else if (grab) {
            try_place(gs);
        }
    } else if (grab && !p->on_ladder) {
        try_pick_up(gs);
    }
}

void crate_release_carried(GameState *gs) {
    Companion *p = &gs->player;
    Entity *e = pool_get(&gs->entities, p->carrying);
    p->carrying = ENTITY_NONE;
    if (e && !e->pending_destroy && e->as.crate.state == CRATE_CARRIED) {
        e->as.crate.state = CRATE_FALLING;
        e->body.vel = vec2(0.0f, 0.0f);
    }
}
