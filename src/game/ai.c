#include "game/ai.h"

#include "game/combat.h"
#include "game/pad.h"
#include "game/tuning.h"

#include <math.h>

#define BAN_TICKS SECONDS_TO_TICKS(TUNING_AI_BAN_S)
#define GIVE_UP_TICKS SECONDS_TO_TICKS(TUNING_AI_GIVE_UP_S)

// ---- Perception --------------------------------------------------------------------------------

static Vec2 shoulder_of(const Companion *c) {
    return vec2(c->body.pos.x + c->body.size.x * 0.5f, c->body.pos.y + TUNING_MUZZLE_HEIGHT);
}

typedef struct Enemy {
    bool found;
    Vec2 pos;    // center of the target
    f32 dist;
} Enemy;

static void consider(const GameState *gs, const Companion *c, Rect rect, Enemy *best) {
    const Vec2 sh = shoulder_of(c);
    const Vec2 t = vec2(rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f);
    const f32 dx = t.x - sh.x, dy = t.y - sh.y;
    const f32 dist = sqrtf(dx * dx + dy * dy);
    if (dist > TUNING_AI_RANGE || fabsf(dy) > TUNING_AI_RANGE_Y || (best->found && dist >= best->dist)) {
        return;
    }
    if (!combat_line_clear(gs->level, sh, t)) {
        return;
    }
    *best = (Enemy){true, t, dist};
}

// The closest enemy in range and in sight: soldiers of other teams, and dummies.
static Enemy find_enemy(const GameState *gs, const Soldier *me) {
    Enemy best = {false, vec2(0, 0), 0.0f};
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        const Soldier *s = &gs->soldiers[i];
        if (s->alive && s->team != me->team && s->c.hp > 0) {
            consider(gs, &me->c, body_rect(&s->c.body), &best);
        }
    }
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type == ENT_DUMMY && !e->pending_destroy && e->as.dummy.hp > 0) {
            consider(gs, &me->c, body_rect(&e->body), &best);
        }
    }
    return best;
}

static f32 center_x(const Companion *c) {
    return c->body.pos.x + c->body.size.x * 0.5f;
}

static f32 feet_y(const Companion *c) {
    return c->body.pos.y + c->body.size.y;
}

// Where the team's pad is (center of its cells), or the spawn when it has none.
static f32 home_x(const GameState *gs, i32 team) {
    if (gs->level->pads[team].present) {
        const Rect r = pad_rect(gs, team);
        return r.x + r.w * 0.5f;
    }
    return gs->teams[team].spawn.x;
}

// ---- Movement ----------------------------------------------------------------------------------

typedef struct Plan {
    f32 target_x;
    f32 band;      // stop when closer than this
    bool carrying_ok; // true: may shoot / punch (never while carrying: fire would throw the crate)
    u32 extra;     // keys to add (grab...)
} Plan;

static u32 walk(Soldier *s, const Level *lv, f32 target_x, f32 band) {
    Walker *w = &s->ai.walker;
    const f32 here = center_x(&s->c);
    w->target_x = fabsf(target_x - here) <= band ? here : target_x;
    InputFrame tmp = {0};
    walker_think(w, lv, &s->c, &tmp);
    return tmp.down;
}

// ---- Collecting crates -------------------------------------------------------------------------

static bool in_pad(const GameState *gs, i32 team, const Entity *crate) {
    const PadSite *site = &gs->level->pads[team];
    if (!site->present) {
        return false;
    }
    const i32 cx = crate->as.crate.cell_x, cy = crate->as.crate.cell_y;
    return cx >= site->cell_x && cx < site->cell_x + PAD_COLS && cy > site->cell_y - PAD_ROWS && cy <= site->cell_y;
}

static u32 pad_crate_count(const GameState *gs, i32 team) {
    PadGrid grid;
    pad_read_grid(gs, team, &grid, NULL);
    u32 n = 0;
    for (i32 r = 0; r < PAD_ROWS; r++) {
        for (i32 c = 0; c < PAD_COLS; c++) {
            n += grid.cells[r][c] != PAD_EMPTY ? 1u : 0u;
        }
    }
    return n;
}

static EntityId slot_id(const GameState *gs, u16 slot) {
    return (EntityId){slot, gs->entities.slots[slot].generation};
}

// The nearest crate on the soldier's floor that is free to take (top of its stack, not already on the pad).
static EntityId pick_crate(const GameState *gs, const Soldier *s) {
    EntityId best = ENTITY_NONE;
    f32 best_d = 0.0f;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type != ENT_CRATE || e->pending_destroy || e->as.crate.state != CRATE_RESTING) {
            continue;
        }
        if (entity_id_eq(slot_id(gs, i), s->ai.banned) && s->ai.banned_ticks > 0) {
            continue;
        }
        if (in_pad(gs, s->team, e) || level_dyn_solid(gs->level, e->as.crate.cell_x, e->as.crate.cell_y - 1)) {
            continue;
        }
        if (fabsf(e->body.pos.y + e->body.size.y - feet_y(&s->c)) > (f32)(2 * TILE_SIZE)) {
            continue; // another floor: no pathfinding yet
        }
        const f32 d = fabsf(e->body.pos.x + e->body.size.x * 0.5f - center_x(&s->c));
        if (!entity_id_valid(best) || d < best_d) {
            best = slot_id(gs, i);
            best_d = d;
        }
    }
    return best;
}

// Which column of the pad to fill next: the far one first (seen from where the soldier comes in), so that
// the soldier can keep walking in; at most two crates per column, and nothing in the column it will stand in.
static i32 pick_column(const GameState *gs, i32 team, i32 dir) {
    PadGrid grid;
    pad_read_grid(gs, team, &grid, NULL);
    i32 count[PAD_COLS] = {0};
    for (i32 c = 0; c < PAD_COLS; c++) {
        for (i32 r = 0; r < PAD_ROWS; r++) {
            count[c] += grid.cells[r][c] != PAD_EMPTY ? 1 : 0;
        }
    }
    for (i32 k = 0; k < PAD_COLS; k++) {
        const i32 c = dir > 0 ? PAD_COLS - 1 - k : k; // far column first
        const i32 behind = c - dir;                   // the column the soldier stands in
        if (count[c] < PAD_ROWS && (behind < 0 || behind >= PAD_COLS || count[behind] == 0)) {
            return c;
        }
    }
    return -1;
}

static void ban(Soldier *s, EntityId crate) {
    s->ai.banned = crate;
    s->ai.banned_ticks = BAN_TICKS;
    s->ai.crate = ENTITY_NONE;
    s->ai.collect = COLLECT_SEEK;
    s->ai.state_ticks = 0;
}

// Walks to `x` and, once there, faces `dir` and presses grab every other tick. Returns the keys.
static u32 act_at(Soldier *s, const Level *lv, f32 x, i32 dir) {
    u32 mask = walk(s, lv, x, 0.0f);
    if (walker_arrived(&s->ai.walker, &s->c)) {
        if (s->c.facing != dir) {
            mask |= action_bit(dir > 0 ? ACT_RIGHT : ACT_LEFT); // a tick of turning
        } else if ((s->ai.clock & 1u) == 0) {
            mask |= action_bit(ACT_GRAB);
        }
    }
    return mask;
}

static u32 collect_think(GameState *gs, Soldier *s) {
    SoldierAi *ai = &s->ai;
    Companion *c = &s->c;
    const bool carrying = entity_id_valid(c->carrying);
    if (ai->banned_ticks > 0) {
        ai->banned_ticks--;
    }
    ai->state_ticks++;
    if (carrying && ai->collect != COLLECT_DELIVER) {
        ai->collect = COLLECT_DELIVER;
        ai->state_ticks = 0;
        ai->deliver_dir = center_x(c) < home_x(gs, s->team) ? 1 : -1;
    } else if (!carrying && ai->collect == COLLECT_DELIVER) {
        ai->collect = COLLECT_SEEK;
        ai->state_ticks = 0;
    }
    const bool has_pad = gs->level->pads[s->team].present;
    if (!has_pad) {
        return walk(s, gs->level, home_x(gs, s->team), TUNING_AI_GUARD_BAND);
    }

    switch (ai->collect) {
    case COLLECT_SEEK:
        if (pad_crate_count(gs, s->team) < PAD_ROWS * PAD_COLS) {
            const EntityId id = pick_crate(gs, s);
            if (entity_id_valid(id)) {
                ai->crate = id;
                ai->collect = COLLECT_FETCH;
                ai->state_ticks = 0;
                return 0;
            }
        }
        return walk(s, gs->level, home_x(gs, s->team), TUNING_AI_GUARD_BAND); // nothing to do: wait at the pad
    case COLLECT_FETCH: {
        const Entity *e = pool_get(&gs->entities, ai->crate);
        if (!e || e->pending_destroy || e->as.crate.state != CRATE_RESTING || ai->state_ticks > GIVE_UP_TICKS ||
            walker_stuck(&ai->walker)) {
            ban(s, ai->crate);
            return 0;
        }
        const bool right = e->body.pos.x + e->body.size.x * 0.5f > center_x(c);
        // Stand so that the crate is in the pickup zone in front of the body.
        const f32 x = right ? e->body.pos.x - 10.0f : e->body.pos.x + TUNING_CRATE_SIZE + 10.0f;
        return act_at(s, gs->level, x, right ? 1 : -1);
    }
    case COLLECT_DELIVER: {
        const i32 dir = ai->deliver_dir != 0 ? ai->deliver_dir : 1;
        const i32 col = pick_column(gs, s->team, dir);
        if (col < 0 || ai->state_ticks > GIVE_UP_TICKS) {
            // Nowhere sensible to put it: let it go here (the grab key puts a carried crate down).
            return (ai->clock & 1u) == 0 ? action_bit(ACT_GRAB) : 0u;
        }
        // The body's front edge just before the column: the crate lands in it.
        const i32 abs_col = gs->level->pads[s->team].cell_x + col;
        const f32 x = dir > 0 ? (f32)(abs_col * TILE_SIZE) - 8.0f : (f32)((abs_col + 1) * TILE_SIZE) + 8.0f;
        return act_at(s, gs->level, x, dir);
    }
    }
    return 0;
}

// ---- Thinking ----------------------------------------------------------------------------------

static bool explosive_too_close(const WeaponDef *def, f32 dist) {
    return def->radius > 0.0f && dist < def->radius + 8.0f;
}

static const Soldier *leader_of(const GameState *gs, i32 team) {
    return (gs->controlled >= 0 && gs->soldiers[gs->controlled].team == team) ? &gs->soldiers[gs->controlled] : NULL;
}

static bool want_weapon(const GameState *gs, const Soldier *s, f32 *x) {
    if (s->c.weapon != WEAPON_FISTS || entity_id_valid(s->c.carrying)) {
        return false;
    }
    f32 best = TUNING_AI_WEAPON_RANGE;
    bool found = false;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type != ENT_WEAPON || e->pending_destroy) {
            continue;
        }
        const f32 d = fabsf(e->body.pos.x + e->body.size.x * 0.5f - center_x(&s->c));
        if (d < best && fabsf(e->body.pos.y + e->body.size.y - feet_y(&s->c)) <= (f32)TILE_SIZE) {
            best = d;
            *x = e->body.pos.x + e->body.size.x * 0.5f;
            found = true;
        }
    }
    return found;
}

void ai_think(GameState *gs, i32 index, InputFrame *out) {
    Soldier *s = &gs->soldiers[index];
    const Companion *c = &s->c;
    const Order order = gs->teams[s->team].order;
    const bool carrying = entity_id_valid(c->carrying);
    const Enemy enemy = find_enemy(gs, s);

    u32 mask = 0;
    f32 wx;
    if (want_weapon(gs, s, &wx)) {
        mask = walk(s, gs->level, wx, 0.0f);
    } else if (!carrying && c->weapon == WEAPON_FISTS && enemy.found && enemy.dist < TUNING_AI_CHASE_RANGE) {
        mask = walk(s, gs->level, enemy.pos.x, 8.0f); // nothing but fists: get close
    } else {
        switch (order) {
        case ORDER_FOLLOW: {
            const Soldier *leader = leader_of(gs, s->team);
            mask = walk(s, gs->level, leader ? center_x(&leader->c) : home_x(gs, s->team),
                        leader ? TUNING_AI_FOLLOW_BAND : TUNING_AI_GUARD_BAND);
            break;
        }
        case ORDER_GUARD:
            mask = walk(s, gs->level, home_x(gs, s->team), TUNING_AI_GUARD_BAND);
            break;
        case ORDER_COLLECT:
            mask = collect_think(gs, s);
            break;
        case ORDER_ATTACK: {
            f32 tx = center_x(c);
            f32 best = 1e9f;
            for (i32 t = 0; t < MAX_TEAMS; t++) {
                if (t != s->team && gs->level->pads[t].present) {
                    const f32 x = home_x(gs, t);
                    if (fabsf(x - center_x(c)) < best) {
                        best = fabsf(x - center_x(c));
                        tx = x;
                    }
                }
            }
            mask = walk(s, gs->level, tx, TUNING_AI_GUARD_BAND);
            break;
        }
        default:
            break;
        }
    }

    // Shooting: at the closest enemy in sight; never while carrying (fire would throw the crate).
    bool aim = false;
    if (enemy.found && !carrying) {
        const WeaponDef *def = weapon_def(c->weapon);
        aim = true;
        const bool melee_ok = def->shot != SHOT_MELEE || enemy.dist <= def->reach + 8.0f;
        if (melee_ok && !explosive_too_close(def, enemy.dist) && (def->automatic || (s->ai.clock & 1u) == 0)) {
            mask |= action_bit(ACT_FIRE);
        }
    }
    input_frame_update(out, mask);
    out->aim_active = aim;
    out->aim = enemy.pos;
}
