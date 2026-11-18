#include "game/ai.h"

#include "game/base.h"
#include "game/combat.h"
#include "game/goal.h"
#include "game/pad.h"
#include "game/tuning.h"

#include <math.h>

#define BAN_TICKS SECONDS_TO_TICKS(TUNING_AI_BAN_S)
#define GIVE_UP_TICKS SECONDS_TO_TICKS(TUNING_AI_GIVE_UP_S)
#define ARRIVE_TOL 3.0f
#define SAME_FLOOR (1.5f * TILE_SIZE) // feet this close in height: the same floor

// ---- Perception --------------------------------------------------------------------------------

static Vec2 shoulder_of(const Companion *c) {
    return vec2(c->body.pos.x + c->body.size.x * 0.5f, c->body.pos.y + TUNING_MUZZLE_HEIGHT);
}

typedef struct Enemy {
    bool found;
    bool soldier; // a soldier (not a dummy, not a base)
    bool base;
    Vec2 pos;    // center of the target
    f32 feet_y;
    f32 dist;
} Enemy;

static void consider(const GameState *gs, const Companion *c, Rect rect, bool soldier, Enemy *best) {
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
    *best = (Enemy){true, soldier, false, t, rect.y + rect.h, dist};
}

// The closest enemy in range and in sight: soldiers of other teams, and dummies; else an enemy base.
static Enemy find_enemy(const GameState *gs, const Soldier *me) {
    Enemy best = {false, false, false, vec2(0, 0), 0.0f, 0.0f};
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        const Soldier *s = &gs->soldiers[i];
        if (s->alive && s->team != me->team && s->c.hp > 0) {
            consider(gs, &me->c, body_rect(&s->c.body), true, &best);
        }
    }
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type == ENT_DUMMY && !e->pending_destroy && e->as.dummy.hp > 0) {
            consider(gs, &me->c, body_rect(&e->body), false, &best);
        }
    }
    for (i32 t = 0; t < MAX_TEAMS && !best.found; t++) { // buildings only when nobody fights back
        if (t != me->team && base_alive(gs, t)) {
            consider(gs, &me->c, base_rect(gs, t), false, &best);
            best.base = best.found;
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

// Where the team's pad is (center of its cells, at floor level), or the spawn when it has none.
static Vec2 home_feet(const GameState *gs, i32 team) {
    if (gs->level->pads[team].present) {
        const Rect r = pad_rect(gs, team);
        return vec2(r.x + r.w * 0.5f, r.y + r.h);
    }
    return gs->teams[team].spawn;
}

// Where a soldier with nothing else to do stands: in front of its base (the spawn), not on the pad, where it
// would keep the crates from settling.
static Vec2 guard_post(const GameState *gs, i32 team) {
    return gs->teams[team].spawn;
}

static EntityId slot_id(const GameState *gs, u16 slot) {
    return (EntityId){slot, gs->entities.slots[slot].generation};
}

// ---- Movement ----------------------------------------------------------------------------------

typedef struct Trip {
    u32 mask;      // keys for this tick
    bool arrived;  // close enough (within `band`, or standing still on the spot when band is 0)
    bool no_path;  // the goal cannot be reached
} Trip;

// Straight at an x, hopping small obstacles: what the soldiers do on a level without navigation graph.
static u32 walk(Soldier *s, const Level *lv, f32 target_x, f32 band) {
    Walker *w = &s->ai.walker;
    const f32 here = center_x(&s->c);
    w->target_x = fabsf(target_x - here) <= band ? here : target_x;
    InputFrame tmp = {0};
    walker_think(w, lv, &s->c, &tmp);
    return tmp.down;
}

// Travels to a spot (where the feet should be). With a `band`, being that close on the same floor is enough.
static Trip go_to(GameState *gs, Soldier *s, Vec2 goal, f32 band) {
    Trip t = {0, false, false};
    const Companion *c = &s->c;
    const f32 dx = goal.x - center_x(c);
    if (band > 0.0f && fabsf(goal.y - feet_y(c)) <= SAME_FLOOR && fabsf(dx) <= band) {
        t.arrived = true;
        return t;
    }
    if (!gs->nav.built) {
        nav_build(&gs->nav, gs->level);
    }
    if (!gs->nav.ok) {
        t.mask = walk(s, gs->level, goal.x, band);
        t.arrived = fabsf(dx) <= (band > 0.0f ? band : ARRIVE_TOL);
        return t;
    }
    u32 mask = 0;
    switch (nav_steer(&gs->nav, &s->ai.nav, gs->level, c, goal, &mask)) {
    case NAV_MOVING:
        t.mask = mask;
        break;
    case NAV_ARRIVED: // on the right node: the last few pixels
        t.arrived = band > 0.0f ? fabsf(dx) <= band : nav_settled_at(c, goal.x, ARRIVE_TOL);
        t.mask = t.arrived ? 0u : nav_steer_x(c, goal.x);
        break;
    case NAV_NO_PATH:
        t.no_path = true;
        break;
    }
    return t;
}

// ---- Collecting crates -------------------------------------------------------------------------

typedef enum CrateSource {
    SOURCE_FREE,      // lying around, in no pad
    SOURCE_ENEMY_PAD, // on the pad of another team
} CrateSource;

static bool in_enemy_pad(const GameState *gs, i32 team, const Entity *crate) {
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        if (t != team && pad_holds_crate(gs, t, crate)) {
            return true;
        }
    }
    return false;
}

// The top of a stack: only that one can be taken.
static bool takeable(const GameState *gs, const Entity *e) {
    return e->type == ENT_CRATE && !e->pending_destroy && e->as.crate.state == CRATE_RESTING &&
           !level_dyn_solid(gs->level, e->as.crate.cell_x, e->as.crate.cell_y - 1);
}

static bool is_banned(const SoldierAi *ai, EntityId crate) {
    for (u32 k = 0; k < AI_BAN_SLOTS; k++) {
        if (ai->banned_ticks[k] > 0 && entity_id_eq(ai->banned[k], crate)) {
            return true;
        }
    }
    return false;
}

// Forgets about a crate for a while (in the slot that would be free first).
static void ban_crate(SoldierAi *ai, EntityId crate) {
    u32 slot = 0;
    for (u32 k = 0; k < AI_BAN_SLOTS; k++) {
        if (entity_id_eq(ai->banned[k], crate)) {
            slot = k;
            break;
        }
        if (ai->banned_ticks[k] < ai->banned_ticks[slot]) {
            slot = k;
        }
    }
    ai->banned[slot] = crate;
    ai->banned_ticks[slot] = BAN_TICKS;
}

static void tick_bans(SoldierAi *ai) {
    for (u32 k = 0; k < AI_BAN_SLOTS; k++) {
        if (ai->banned_ticks[k] > 0) {
            ai->banned_ticks[k]--;
        }
    }
}

static bool fetched_by_a_mate(const GameState *gs, const Soldier *me, EntityId crate) {
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        const Soldier *o = &gs->soldiers[i];
        if (o != me && o->alive && o->team == me->team && o->ai.collect == COLLECT_FETCH &&
            entity_id_eq(o->ai.crate, crate)) {
            return true;
        }
    }
    return false;
}

// Which crates a soldier looks for. Only the goal (game/goal.h) asks for a colour.
typedef enum CrateWant {
    WANT_ANY,
    WANT_RED,
    WANT_BLUE,
    WANT_NONE, // no fallback
} CrateWant;

static bool wanted_colour(const Entity *e, CrateWant want) {
    return want == WANT_ANY || (want == WANT_RED && e->as.crate.kind == CRATE_RED) ||
           (want == WANT_BLUE && e->as.crate.kind == CRATE_BLUE);
}

// The nearest crate of a kind that this soldier could take (not banned, not already someone's).
#define CRATE_HALF (TUNING_CRATE_SIZE * 0.5f)

static EntityId nearest_crate(const GameState *gs, const Soldier *s, CrateSource source, CrateWant want, const bool side_ok[2]) {
    const f32 home_x = home_feet(gs, s->team).x;
    EntityId best = ENTITY_NONE;
    f32 best_score = 0.0f;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (!takeable(gs, e) || pad_holds_crate(gs, s->team, e) || !wanted_colour(e, want)) {
            continue;
        }
        if (!side_ok[e->body.pos.x + CRATE_HALF < home_x ? 0 : 1]) {
            continue; // it would be brought in from a side of the pad that cannot take it
        }
        const EntityId id = slot_id(gs, i);
        if (is_banned(&s->ai, id) || fetched_by_a_mate(gs, s, id)) {
            continue;
        }
        const bool enemy_pad = in_enemy_pad(gs, s->team, e);
        if (enemy_pad != (source == SOURCE_ENEMY_PAD)) {
            continue;
        }
        const f32 score = fabsf(e->body.pos.x + e->body.size.x * 0.5f - center_x(&s->c)) +
                          3.0f * fabsf(e->body.pos.y + e->body.size.y - feet_y(&s->c));
        if (!entity_id_valid(best) || score < best_score) {
            best = id;
            best_score = score;
        }
    }
    return best;
}

// Where to stand to take a crate (feet position) and which way to face: on the side it is easier to reach,
// on a floor that exists, with the crate within the reach of the body (its own row, or the row above the feet).
// False when there is no such place (the top of a high stack).
static bool crate_stand(const GameState *gs, const Entity *e, f32 from_x, Vec2 *spot, i32 *dir) {
    const f32 left_x = e->body.pos.x - 10.0f; // stands on its left, faces right
    const f32 right_x = e->body.pos.x + TUNING_CRATE_SIZE + 10.0f;
    const bool left_first = fabsf(left_x - from_x) <= fabsf(right_x - from_x);
    for (i32 k = 0; k < 2; k++) {
        const bool left = (k == 0) == left_first;
        const f32 x = left ? left_x : right_x;
        // Its own row, or the one under it (the top of a stack of two is taken from the floor).
        for (i32 row = e->as.crate.cell_y; row <= e->as.crate.cell_y + 1; row++) {
            const i32 tx = level_px_to_tile(x);
            if (nav_find(&gs->nav, tx, row, NAV_GROUND) >= 0 && !level_dyn_solid(gs->level, tx, row) &&
                !level_dyn_solid(gs->level, tx, row - 1)) { // and nothing (another crate) where the body would be
                *dir = left ? 1 : -1;
                *spot = vec2(x, (f32)((row + 1) * TILE_SIZE));
                return true;
            }
        }
    }
    return false;
}

// Could the soldier fetch that crate and bring it home? It carries it all the way, so no ladders and no high jumps.
static bool deliverable(GameState *gs, const Soldier *s, const Entity *crate) {
    if (!gs->nav.ok) {
        return true;
    }
    i32 dir;
    Vec2 spot;
    if (!crate_stand(gs, crate, center_x(&s->c), &spot, &dir)) {
        return false;
    }
    const i32 from = nav_nearest(&gs->nav, spot);
    const i32 to = nav_nearest(&gs->nav, home_feet(gs, s->team));
    u16 path[NAV_PATH_MAX];
    return nav_path(&gs->nav, from, to, true, -1, path, NAV_PATH_MAX) >= 0;
}

// The nearest crate of a kind that this soldier can fetch and bring home. The ones that cannot be brought home
// are forgotten for a while.
// `side_ok[0]`: crates left of the pad may be fetched, `[1]`: crates on its right.
static EntityId pick_crate(GameState *gs, Soldier *s, CrateSource source, CrateWant want, const bool side_ok[2]) {
    if (want == WANT_NONE) {
        return ENTITY_NONE;
    }
    for (u32 attempt = 0; attempt < AI_BAN_SLOTS; attempt++) {
        const EntityId id = nearest_crate(gs, s, source, want, side_ok);
        if (!entity_id_valid(id)) {
            break;
        }
        if (deliverable(gs, s, pool_get_const(&gs->entities, id))) {
            return id;
        }
        ban_crate(&s->ai, id);
    }
    return ENTITY_NONE;
}

// ---- Working towards the team's build goal -----------------------------------------------------

// What the other soldiers of the team already have under way: crates carried (by anyone) or being fetched (by the AI).
typedef struct Inflight {
    u32 total, blue;
} Inflight;

static Inflight inflight(const GameState *gs, const Soldier *me) {
    Inflight f = {0, 0};
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        const Soldier *o = &gs->soldiers[i];
        if (o == me || !o->alive || o->team != me->team) {
            continue;
        }
        EntityId crate = o->c.carrying;
        if (!entity_id_valid(crate) && o->ai.collect == COLLECT_FETCH && i != gs->controlled) {
            crate = o->ai.crate;
        }
        const Entity *e = pool_get_const(&gs->entities, crate);
        if (e && e->type == ENT_CRATE && !e->pending_destroy) {
            f.total++;
            f.blue += e->as.crate.kind == CRATE_BLUE ? 1u : 0u;
        }
    }
    return f;
}

// Where to look first and, failing that, where else. A red goal looks for red crates, then takes blue ones. A blue goal
// looks for a blue crate until one is on its way (red ones may fill the other cells meanwhile, but not the last one).
static void crate_wishes(const BuildGoal *goal, const GoalPlan *plan, const Inflight *fl, u32 need, CrateWant *first,
                         CrateWant *fallback) {
    const bool blue_secured = !plan->needs_blue || fl->blue > 0;
    if (goal->blue && !blue_secured) {
        *first = WANT_BLUE;
        *fallback = need >= 2 ? WANT_RED : WANT_NONE;
    } else {
        *first = WANT_RED;
        *fallback = WANT_ANY;
    }
}

// The column of the pad to put the crate in under a goal: one where the next crate is wanted, the far ones first (as in
// pick_column), skipping the places the mates on their way already count on. The plan says which side to come from.
// A mate delivering to a farther column goes first (it stands in the columns before it).
static i32 pick_goal_column(const GameState *gs, const Soldier *me, const GoalPlan *plan) {
    u8 claimed[PAD_COLS] = {0};
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        const Soldier *o = &gs->soldiers[i];
        if (o != me && o->alive && o->team == me->team && o->ai.collect == COLLECT_DELIVER &&
            entity_id_valid(o->c.carrying) && o->ai.deliver_col >= 0 && o->ai.deliver_col < PAD_COLS) {
            claimed[o->ai.deliver_col]++;
        }
    }
    const i32 d = plan->side != 0 ? plan->side : 1;
    for (i32 i = 0; i < PAD_COLS; i++) {
        const i32 c = d > 0 ? PAD_COLS - 1 - i : i;
        const i32 level = plan->filled[c] + claimed[c];
        if (level >= PAD_ROWS || !(plan->wanted[c] & (1u << level))) {
            continue;
        }
        bool blocked = false; // a mate bringing a crate to a column farther in
        for (i32 k = d > 0 ? c + 1 : 0; k < (d > 0 ? PAD_COLS : c); k++) {
            blocked |= claimed[k] > 0;
        }
        if (!blocked) {
            return c;
        }
    }
    return -1;
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
    ban_crate(&s->ai, crate);
    s->ai.crate = ENTITY_NONE;
    s->ai.collect = COLLECT_SEEK;
    s->ai.state_ticks = 0;
}

// Goes to `spot` and, once there, faces `dir` and presses grab every other tick.
static u32 act_at(GameState *gs, Soldier *s, Vec2 spot, i32 dir, bool *no_path) {
    const Trip t = go_to(gs, s, spot, 0.0f);
    *no_path = t.no_path;
    u32 mask = t.mask;
    if (t.arrived) {
        if (s->c.facing != dir) {
            mask |= action_bit(dir > 0 ? ACT_RIGHT : ACT_LEFT); // a tick of turning
        } else if ((s->ai.clock & 1u) == 0) {
            mask |= action_bit(ACT_GRAB);
        }
    }
    return mask;
}

// Fetch crates and bring them to the pad. `steal`: take them from the enemy pads first.
static u32 collect_think(GameState *gs, Soldier *s, bool steal, AiMode *mode) {
    SoldierAi *ai = &s->ai;
    Companion *c = &s->c;
    const bool carrying = entity_id_valid(c->carrying);
    const Vec2 home = home_feet(gs, s->team);
    tick_bans(ai);
    ai->state_ticks++;
    if (carrying && ai->collect != COLLECT_DELIVER) {
        ai->collect = COLLECT_DELIVER;
        ai->state_ticks = 0;
        ai->deliver_dir = center_x(c) < home.x ? 1 : -1;
        ai->deliver_col = -1;
    } else if (!carrying && ai->collect == COLLECT_DELIVER) {
        ai->collect = COLLECT_SEEK;
        ai->state_ticks = 0;
    }
    *mode = AI_GUARD;
    if (!gs->level->pads[s->team].present) {
        return go_to(gs, s, guard_post(gs, s->team), TUNING_AI_GUARD_BAND).mask;
    }
    if (!gs->nav.built) {
        nav_build(&gs->nav, gs->level);
    }

    bool no_path = false;
    switch (ai->collect) {
    case COLLECT_SEEK: {
        GoalPlan plan, from_left, from_right; // the crates are brought in from the side they are on
        if (!steal) {
            goal_plan(gs, s->team, 1, &from_left);
            goal_plan(gs, s->team, -1, &from_right);
            plan = from_left.feasible ? from_left : from_right;
        }
        if (!steal && plan.valid) { // a goal: only what the shape still needs, in the colour it asks for
            const Inflight fl = inflight(gs, s);
            const u32 need = plan.missing > fl.total ? plan.missing - fl.total : 0;
            const bool side_ok[2] = {from_left.feasible, from_right.feasible};
            CrateWant first = WANT_ANY, fallback = WANT_NONE;
            crate_wishes(&gs->teams[s->team].goal, &plan, &fl, need, &first, &fallback);
            EntityId id = ENTITY_NONE;
            if (plan.feasible && !plan.buildable && need > 0) {
                id = pick_crate(gs, s, SOURCE_FREE, first, side_ok);
                if (!entity_id_valid(id)) {
                    id = pick_crate(gs, s, SOURCE_FREE, fallback, side_ok);
                }
            }
            if (entity_id_valid(id)) {
                ai->crate = id;
                ai->collect = COLLECT_FETCH;
                ai->state_ticks = 0;
                return 0;
            }
        } else if (pad_crate_count(gs, s->team) < PAD_CELLS) {
            EntityId id = ENTITY_NONE;
            static const bool EVERY_SIDE[2] = {true, true};
            if (steal) {
                id = pick_crate(gs, s, SOURCE_ENEMY_PAD, WANT_ANY, EVERY_SIDE);
            }
            if (!entity_id_valid(id)) {
                id = pick_crate(gs, s, SOURCE_FREE, WANT_ANY, EVERY_SIDE);
            }
            if (entity_id_valid(id)) {
                ai->crate = id;
                ai->collect = COLLECT_FETCH;
                ai->state_ticks = 0;
                return 0;
            }
        }
        return go_to(gs, s, guard_post(gs, s->team), TUNING_AI_GUARD_BAND).mask; // nothing to do: wait at the base
    }
    case COLLECT_FETCH: {
        const Entity *e = pool_get_const(&gs->entities, ai->crate);
        if (!e || !takeable(gs, e) || ai->state_ticks > GIVE_UP_TICKS) {
            ban(s, ai->crate);
            return 0;
        }
        *mode = in_enemy_pad(gs, s->team, e) ? AI_STEAL : AI_FETCH;
        i32 dir;
        Vec2 spot;
        if (!crate_stand(gs, e, center_x(c), &spot, &dir)) {
            ban(s, ai->crate); // a stack too high to reach
            return 0;
        }
        const u32 mask = act_at(gs, s, spot, dir, &no_path);
        if (no_path) {
            ban(s, ai->crate);
            return 0;
        }
        return mask;
    }
    case COLLECT_DELIVER: {
        *mode = AI_DELIVER;
        i32 dir = ai->deliver_dir != 0 ? ai->deliver_dir : 1;
        GoalPlan plan;
        if (!steal) {
            goal_plan(gs, s->team, dir, &plan);
        }
        const bool goal = !steal && plan.valid;
        i32 col;
        if (goal) {
            col = pick_goal_column(gs, s, &plan);
            if (col >= 0) {
                dir = plan.side;
                ai->deliver_dir = dir;
            }
            ai->deliver_col = col;
        } else {
            col = pick_column(gs, s->team, dir);
        }
        u32 mask = 0;
        if (goal && col < 0 && ai->state_ticks <= GIVE_UP_TICKS) {
            return 0; // nothing is wanted just now (a mate is placing the crate it needs): wait with the crate
        }
        if (col >= 0 && ai->state_ticks <= GIVE_UP_TICKS) {
            // The body's front edge just before the column: the crate lands in it.
            const i32 abs_col = gs->level->pads[s->team].cell_x + col;
            const f32 x = dir > 0 ? (f32)(abs_col * TILE_SIZE) - 8.0f : (f32)((abs_col + 1) * TILE_SIZE) + 8.0f;
            mask = act_at(gs, s, vec2(x, home.y), dir, &no_path);
        }
        if (col < 0 || ai->state_ticks > GIVE_UP_TICKS || no_path) {
            // Nowhere sensible to put it, or no way there: let it go here (grab puts a carried crate down)
            // and do not pick it up again right away.
            ban_crate(ai, c->carrying);
            return (ai->clock & 1u) == 0 ? action_bit(ACT_GRAB) : 0u;
        }
        return mask;
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

// A weapon lying close by that an unarmed soldier should go and get.
static bool want_weapon(const GameState *gs, const Soldier *s, Vec2 *feet) {
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
        if (d < best && fabsf(e->body.pos.y + e->body.size.y - feet_y(&s->c)) <= TUNING_AI_ARM_RANGE_Y) {
            best = d;
            *feet = vec2(e->body.pos.x + e->body.size.x * 0.5f, e->body.pos.y + e->body.size.y);
            found = true;
        }
    }
    return found;
}

// Where to attack the nearest other team still in the game: its base while it stands, else its pad
// (where its soldiers gather).
static bool enemy_base(const GameState *gs, const Soldier *s, Vec2 *feet) {
    bool found = false;
    f32 best = 0.0f;
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        const Team *team = &gs->teams[t];
        if (t == s->team || !team->active || team->eliminated || (!team->playing && !base_alive(gs, t))) {
            continue;
        }
        Vec2 h = home_feet(gs, t);
        if (base_alive(gs, t)) { // in front of the building, on the side the attacker comes from
            const Rect r = base_rect(gs, t);
            const f32 side = center_x(&s->c) < r.x + r.w * 0.5f ? -1.0f : 1.0f;
            h = vec2(r.x + r.w * 0.5f + side * (r.w * 0.5f + 3.0f * TILE_SIZE), r.y + r.h);
        }
        const f32 score = fabsf(h.x - center_x(&s->c)) + 3.0f * fabsf(h.y - feet_y(&s->c));
        if (!found || score < best) {
            best = score;
            *feet = h;
            found = true;
        }
    }
    return found;
}

static AiMode order_think(GameState *gs, Soldier *s, Order order, u32 *mask) {
    const Vec2 home = guard_post(gs, s->team);
    switch (order) {
    case ORDER_FOLLOW: {
        const Soldier *leader = leader_of(gs, s->team);
        if (leader) {
            *mask = go_to(gs, s, vec2(center_x(&leader->c), feet_y(&leader->c)), TUNING_AI_FOLLOW_BAND).mask;
            return AI_FOLLOW;
        }
        *mask = go_to(gs, s, home, TUNING_AI_GUARD_BAND).mask;
        return AI_GUARD;
    }
    case ORDER_COLLECT:
    case ORDER_STEAL: {
        AiMode mode;
        *mask = collect_think(gs, s, order == ORDER_STEAL, &mode);
        return mode;
    }
    case ORDER_ATTACK: {
        Vec2 target;
        if (enemy_base(gs, s, &target)) {
            *mask = go_to(gs, s, target, TUNING_AI_GUARD_BAND).mask;
            return AI_ATTACK;
        }
        *mask = go_to(gs, s, home, TUNING_AI_GUARD_BAND).mask;
        return AI_GUARD;
    }
    case ORDER_GUARD:
    default:
        *mask = go_to(gs, s, home, TUNING_AI_GUARD_BAND).mask;
        return AI_GUARD;
    }
}

void ai_think(GameState *gs, i32 index, InputFrame *out) {
    Soldier *s = &gs->soldiers[index];
    const Companion *c = &s->c;
    const Team *team = &gs->teams[s->team];
    const Order order = team->ai ? s->ai.role : team->order;
    const bool carrying = entity_id_valid(c->carrying);
    const Enemy enemy = find_enemy(gs, s);
    const Vec2 home = guard_post(gs, s->team); // a hurt soldier runs back there

    AiMode mode = AI_IDLE;
    u32 mask = 0;
    Vec2 spot;
    const bool hurt = c->hp * 100 < c->max_hp * TUNING_AI_FLEE_HP_PERCENT;
    const bool at_home = fabsf(home.y - feet_y(c)) <= SAME_FLOOR && fabsf(home.x - center_x(c)) <= TUNING_AI_GUARD_BAND;
    // Workers (collecting, stealing) mind their business unless the enemy is on top of them.
    const bool worker = order == ORDER_COLLECT || order == ORDER_STEAL;
    const f32 chase_range = worker ? TUNING_AI_WORKER_CHASE_RANGE : TUNING_AI_CHASE_RANGE;
    Trip trip = {0, false, false};
    if (hurt && !carrying && enemy.found && enemy.dist < TUNING_AI_FLEE_RANGE && !at_home &&
        !(trip = go_to(gs, s, home, TUNING_AI_GUARD_BAND)).no_path) {
        mode = AI_FLEE; // hurt with an enemy close: back to the base (and keep shooting on the way)
        mask = trip.mask;
    } else if (want_weapon(gs, s, &spot) && !(trip = go_to(gs, s, spot, 0.0f)).no_path) {
        mode = AI_ARM;
        mask = trip.mask;
    } else if (!carrying && c->weapon == WEAPON_FISTS && enemy.found && (enemy.soldier || !worker) && enemy.dist < chase_range &&
               !(trip = go_to(gs, s, vec2(enemy.pos.x, enemy.feet_y), 8.0f)).no_path) {
        mode = AI_ATTACK; // nothing but fists: get close
        mask = trip.mask;
    } else {
        mode = order_think(gs, s, order, &mask);
    }
    s->ai.mode = mode;

    // Shooting: at the closest enemy in sight; never while carrying (fire would throw the crate).
    bool aim = false;
    if (enemy.found && !carrying) {
        const WeaponDef *def = weapon_def(c->weapon);
        aim = true;
        const bool melee_ok = def->shot != SHOT_MELEE || enemy.dist <= def->reach + 8.0f;
        if (melee_ok && !explosive_too_close(def, enemy.dist)) {
            mask |= action_bit(ACT_FIRE);
        }
    }
    input_frame_update(out, mask);
    out->aim_active = aim;
    out->aim = enemy.pos;
    if (aim) { // nobody aims perfectly (and two AI teams do not replay the same match for every seed)
        out->aim.x += (rng_f32(&gs->rng) * 2.0f - 1.0f) * TUNING_AI_AIM_ERROR;
        out->aim.y += (rng_f32(&gs->rng) * 2.0f - 1.0f) * TUNING_AI_AIM_ERROR;
    }
}
