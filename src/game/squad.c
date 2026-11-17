#include "game/squad.h"

#include "game/ai.h"
#include "game/combat.h"
#include "game/crate.h"
#include "game/tuning.h"

#include <math.h>
#include <string.h>

// ---- Variants ----------------------------------------------------------------------------------

static const SoldierDef VARIANTS[SOLDIER_VARIANT_COUNT] = {
    [SOLDIER_NORMAL] = {"companion", TUNING_HP_NORMAL, WEAPON_FISTS},
    [SOLDIER_STRONG] = {"strong companion", TUNING_HP_STRONG, WEAPON_FISTS},
    [SOLDIER_HEAVY] = {"heavy companion", TUNING_HP_HEAVY, WEAPON_ROCKET_LAUNCHER},
    [SOLDIER_HEAVY_ARMORED] = {"heavy armored companion", TUNING_HP_HEAVY_ARMORED, WEAPON_ROCKET_LAUNCHER},
};

const SoldierDef *soldier_def(SoldierVariant variant) {
    return &VARIANTS[(variant >= 0 && variant < SOLDIER_VARIANT_COUNT) ? variant : SOLDIER_NORMAL];
}

static const char *const ORDER_NAMES[ORDER_COUNT] = {"follow", "guard", "collect", "attack"};

const char *order_name(Order order) {
    return (order >= 0 && order < ORDER_COUNT) ? ORDER_NAMES[order] : "?";
}

bool soldier_from_product(PadProduct product, SoldierVariant *out) {
    switch (product) {
    case PRODUCT_COMPANION:
        *out = SOLDIER_NORMAL;
        return true;
    case PRODUCT_STRONG_COMPANION:
        *out = SOLDIER_STRONG;
        return true;
    case PRODUCT_HEAVY_COMPANION:
        *out = SOLDIER_HEAVY;
        return true;
    case PRODUCT_HEAVY_ARMORED_COMPANION:
        *out = SOLDIER_HEAVY_ARMORED;
        return true;
    default:
        return false;
    }
}

// ---- Accessors ---------------------------------------------------------------------------------

Companion *game_player(GameState *gs) {
    return gs->controlled >= 0 ? &gs->soldiers[gs->controlled].c : &gs->ghost;
}

const Companion *game_player_const(const GameState *gs) {
    return gs->controlled >= 0 ? &gs->soldiers[gs->controlled].c : &gs->ghost;
}

u32 team_soldier_count(const GameState *gs, i32 team) {
    u32 n = 0;
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        n += (gs->soldiers[i].alive && gs->soldiers[i].team == team) ? 1u : 0u;
    }
    return n;
}

// ---- Spawning ----------------------------------------------------------------------------------

static void init_soldier_state(Soldier *s, const Vec2 feet) {
    const SoldierDef *def = soldier_def(s->variant);
    companion_init(&s->c, feet);
    s->c.hp = s->c.max_hp = def->hp;
    if (def->start_weapon != WEAPON_FISTS) {
        soldier_give_weapon(&s->c, def->start_weapon);
    }
}

static bool body_fits(const GameState *gs, const Companion *c) {
    const Body *b = &c->body;
    const i32 tx0 = level_px_to_tile(b->pos.x + 0.001f), tx1 = level_px_to_tile(b->pos.x + b->size.x - 0.001f);
    const i32 ty0 = level_px_to_tile(b->pos.y + 0.001f), ty1 = level_px_to_tile(b->pos.y + b->size.y - 0.001f);
    for (i32 ty = ty0; ty <= ty1; ty++) {
        for (i32 tx = tx0; tx <= tx1; tx++) {
            if (level_is_solid(gs->level, tx, ty)) {
                return false;
            }
        }
    }
    return true;
}

static i32 spawn_soldier(GameState *gs, i32 team, SoldierVariant variant, Vec2 feet, bool check_room) {
    if (team < 0 || team >= MAX_TEAMS || variant < 0 || variant >= SOLDIER_VARIANT_COUNT) {
        return -1;
    }
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        if (gs->soldiers[i].alive) {
            continue;
        }
        Soldier *s = &gs->soldiers[i];
        memset(s, 0, sizeof(*s));
        s->team = team;
        s->variant = variant;
        init_soldier_state(s, feet);
        if (check_room && !body_fits(gs, &s->c)) {
            memset(s, 0, sizeof(*s));
            return -1;
        }
        s->alive = true;
        s->ai.walker.target_x = body_center(&s->c.body).x;
        gs->teams[team].eliminated = false;
        events_push(&gs->events, EV_SOLDIER_SPAWNED, body_center(&s->c.body), (i32)variant, team);
        if (gs->controlled < 0 && team == PLAYER_TEAM) {
            gs->controlled = i; // the human has nobody: the newcomer is theirs
            events_push(&gs->events, EV_CONTROL_CHANGED, body_center(&s->c.body), i, 0);
        }
        return i;
    }
    return -1;
}

i32 soldier_spawn(GameState *gs, i32 team, SoldierVariant variant, Vec2 feet) {
    return spawn_soldier(gs, team, variant, feet, true);
}

i32 soldier_spawn_at_start(GameState *gs, i32 team, SoldierVariant variant, Vec2 feet) {
    return spawn_soldier(gs, team, variant, feet, false);
}

// ---- Control and orders ------------------------------------------------------------------------

static void set_control(GameState *gs, i32 index) {
    gs->controlled = index;
    events_push(&gs->events, EV_CONTROL_CHANGED, body_center(&gs->soldiers[index].c.body), index, 0);
}

void squad_cycle_control(GameState *gs) {
    if (gs->controlled < 0) {
        return;
    }
    const i32 team = gs->soldiers[gs->controlled].team;
    for (i32 k = 1; k < MAX_SOLDIERS; k++) {
        const i32 i = (gs->controlled + k) % MAX_SOLDIERS;
        if (gs->soldiers[i].alive && gs->soldiers[i].team == team) {
            set_control(gs, i);
            return;
        }
    }
}

void squad_cycle_order(GameState *gs) {
    const i32 team = gs->controlled >= 0 ? gs->soldiers[gs->controlled].team : PLAYER_TEAM;
    Team *t = &gs->teams[team];
    t->order = (Order)((t->order + 1) % ORDER_COUNT);
    events_push(&gs->events, EV_ORDER_CHANGED, body_center(&game_player(gs)->body), (i32)t->order, team);
}

// ---- Death and respawn -------------------------------------------------------------------------

static i32 closest_ally(const GameState *gs, i32 from, i32 team) {
    const Vec2 p = body_center(&gs->soldiers[from].c.body);
    i32 best = -1;
    f32 best_d = 0.0f;
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        const Soldier *s = &gs->soldiers[i];
        if (i == from || !s->alive || s->team != team) {
            continue;
        }
        const Vec2 q = body_center(&s->c.body);
        const f32 d = (q.x - p.x) * (q.x - p.x) + (q.y - p.y) * (q.y - p.y);
        if (best < 0 || d < best_d) {
            best = i;
            best_d = d;
        }
    }
    return best;
}

void soldier_kill(GameState *gs, i32 index) {
    Soldier *s = &gs->soldiers[index];
    if (!s->alive) {
        return;
    }
    gs->combat_stats.deaths++;
    events_push(&gs->events, EV_SOLDIER_DIED, body_center(&s->c.body), s->team, index);
    combat_drop_loot(gs, &s->c);
    s->alive = false;
    if (gs->controlled == index) {
        gs->ghost = s->c;
        gs->ghost.hp = 0;
        const i32 next = closest_ally(gs, index, s->team);
        if (next >= 0) {
            set_control(gs, next);
        } else {
            gs->controlled = -1;
        }
    }
    if (team_soldier_count(gs, s->team) == 0) {
        gs->teams[s->team].eliminated = true;
        events_push(&gs->events, EV_TEAM_ELIMINATED, body_center(&s->c.body), s->team, 0);
    }
}

void soldier_respawn(GameState *gs, i32 index) {
    Soldier *s = &gs->soldiers[index];
    crate_destroy(gs, s->c.carrying); // the crate is lost with its carrier (and so is the weapon)
    init_soldier_state(s, gs->teams[s->team].spawn);
    memset(&s->ai, 0, sizeof(s->ai));
    s->ai.walker.target_x = body_center(&s->c.body).x;
    gs->respawns++;
    events_push(&gs->events, EV_PLAYER_RESPAWNED, s->c.body.pos, index, s->team);
}

// ---- One tick ----------------------------------------------------------------------------------

static u32 crate_actions(const GameState *gs) {
    return gs->crate_stats.picked_up + gs->crate_stats.placed + gs->crate_stats.thrown;
}

void squad_update(GameState *gs, const InputFrame *human) {
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        Soldier *s = &gs->soldiers[i];
        if (!s->alive) {
            continue;
        }
        const InputFrame *in = human;
        if (i != gs->controlled) {
            ai_think(gs, i, &s->input);
            in = &s->input;
        }
        const bool was_carrying = entity_id_valid(s->c.carrying);
        const u32 actions_before = crate_actions(gs);
        companion_update(&s->c, gs->level, in);
        crate_carry_update(gs, &s->c, in);
        // A key a crate action used is not also a shot or a weapon pickup.
        combat_soldier_update(gs, s, in, was_carrying || crate_actions(gs) != actions_before);
        s->ai.clock++;
    }
}

void squad_resolve(GameState *gs) {
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        Soldier *s = &gs->soldiers[i];
        if (!s->alive) {
            continue;
        }
        if (s->c.hp <= 0) {
            soldier_kill(gs, i);
        } else if (companion_in_hazard(&s->c, gs->level)) {
            soldier_respawn(gs, i);
        }
    }
}
