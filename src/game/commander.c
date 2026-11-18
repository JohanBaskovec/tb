#include "game/commander.h"

#include "game/pad.h"
#include "game/squad.h"
#include "game/tuning.h"

#include <math.h>

#define PERIOD SECONDS_TO_TICKS(TUNING_COMMANDER_PERIOD_S)
#define PATIENCE SECONDS_TO_TICKS(TUNING_COMMANDER_PATIENCE_S)
#define WORTH_WAITING_FOR 20 // while the squad is big enough, only products worth at least this are built at once

i32 commander_product_value(PadProduct product) {
    switch (product) {
    case PRODUCT_PISTOL:
        return 3;
    case PRODUCT_UZI:
        return 8;
    case PRODUCT_GRENADES:
        return 5;
    case PRODUCT_STRONG_GRENADES:
        return 8;
    case PRODUCT_COMPANION:
        return 10;
    case PRODUCT_STRONG_COMPANION:
        return 14;
    case PRODUCT_ROCKET_LAUNCHER:
        return 24;
    case PRODUCT_HEAVY_COMPANION:
        return 30;
    case PRODUCT_HEAVY_ARMORED_COMPANION:
        return 40;
    default:
        return 0; // vehicles: not before they exist (M7)
    }
}

static bool is_soldier_product(PadProduct product) {
    SoldierVariant v;
    return soldier_from_product(product, &v);
}

static Vec2 base_of(const GameState *gs, i32 team) {
    if (gs->level->pads[team].present) {
        const Rect r = pad_rect(gs, team);
        return vec2(r.x + r.w * 0.5f, r.y + r.h);
    }
    return gs->teams[team].spawn;
}

// ---- Building ----------------------------------------------------------------------------------

// Builds the best thing the crates on the pad allow, when it is time to.
static void build(GameState *gs, i32 team, u32 squad, u32 pad_crates) {
    Team *t = &gs->teams[team];
    if (pad_crates != t->pad_crates) {
        t->pad_crates = pad_crates;
        t->stagnant_ticks = 0;
    } else {
        t->stagnant_ticks += PERIOD;
    }
    PadOptionList options;
    pad_world_options(gs, team, &options);
    const bool small_squad = squad < TUNING_COMMANDER_MIN_SQUAD;
    const bool out_of_patience = t->stagnant_ticks >= PATIENCE;
    const PadOption *best = NULL;
    i32 best_value = 0;
    for (u32 i = 0; i < options.count; i++) {
        const PadOption *o = &options.items[i];
        i32 value = commander_product_value(o->product);
        const bool soldier = is_soldier_product(o->product);
        if (soldier && squad >= TUNING_COMMANDER_MAX_SQUAD) {
            value = 0; // enough soldiers
        } else if (small_squad && !soldier && !out_of_patience) {
            value = 0; // a thin squad needs soldiers first, not weapons
        }
        if (value > best_value) {
            best = o;
            best_value = value;
        }
    }
    if (best && (small_squad || best_value >= WORTH_WAITING_FOR || out_of_patience)) {
        pad_build(gs, team, best->shape, best->blue);
    }
}

// ---- Roles -------------------------------------------------------------------------------------

typedef struct Situation {
    u32 squad;
    u32 pad_crates;
    u32 free_crates;  // lying around, can be taken
    u32 enemy_crates; // on the pads of other teams, can be taken
    bool threatened;  // an enemy soldier close to the base
} Situation;

static Situation look(const GameState *gs, i32 team) {
    Situation s = {0};
    s.squad = team_soldier_count(gs, team);
    s.pad_crates = pad_crate_count(gs, team);
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type != ENT_CRATE || e->pending_destroy || e->as.crate.state != CRATE_RESTING ||
            level_dyn_solid(gs->level, e->as.crate.cell_x, e->as.crate.cell_y - 1) || pad_holds_crate(gs, team, e)) {
            continue;
        }
        bool enemy_pad = false;
        for (i32 t = 0; t < MAX_TEAMS; t++) {
            enemy_pad |= t != team && pad_holds_crate(gs, t, e);
        }
        if (enemy_pad) {
            s.enemy_crates++;
        } else {
            s.free_crates++;
        }
    }
    s.threatened = gs->teams[team].base_hurt_ticks > 0; // the base is under fire
    const Vec2 home = base_of(gs, team);
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        const Soldier *o = &gs->soldiers[i];
        if (!o->alive || o->team == team) {
            continue;
        }
        const f32 dx = o->c.body.pos.x - home.x, dy = o->c.body.pos.y - home.y;
        s.threatened |= sqrtf(dx * dx + dy * dy) < TUNING_COMMANDER_THREAT_RANGE;
    }
    return s;
}

static u32 clamp_u32(u32 v, u32 lo, u32 hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static bool is_heavy(const Soldier *s) {
    return s->variant == SOLDIER_HEAVY || s->variant == SOLDIER_HEAVY_ARMORED;
}

static void give_roles(GameState *gs, i32 team, const Situation *sit) {
    const bool has_pad = pad_usable(gs, team);
    u32 collect = 0, steal = 0, attack = 0;
    if (has_pad && sit->pad_crates < PAD_CELLS && sit->free_crates > 0) {
        collect = clamp_u32((sit->squad + 1) / 2, 1, 3);
    }
    if (has_pad && sit->enemy_crates > 0 && sit->squad >= 2 && (sit->free_crates == 0 || sit->squad >= 4)) {
        steal = 1;
    }
    if (sit->threatened) {
        collect = MIN(collect, 1u);
        steal = 0;
    } else if (sit->squad >= TUNING_COMMANDER_ATTACK_SQUAD) {
        attack = (sit->squad - MIN(sit->squad, collect + steal)) / 2;
    }

    // Soldiers carrying a crate finish the job; heavy ones lead the offensive; the rest take what is left.
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        Soldier *s = &gs->soldiers[i];
        if (!s->alive || s->team != team || !entity_id_valid(s->c.carrying)) {
            continue;
        }
        if (s->ai.role == ORDER_STEAL) {
            steal = steal > 0 ? steal - 1 : 0;
        } else {
            s->ai.role = ORDER_COLLECT;
            collect = collect > 0 ? collect - 1 : 0;
        }
    }
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        Soldier *s = &gs->soldiers[i];
        if (!s->alive || s->team != team || entity_id_valid(s->c.carrying)) {
            continue;
        }
        if (is_heavy(s) && !sit->threatened && sit->squad >= 2) {
            s->ai.role = ORDER_ATTACK;
            attack = attack > 0 ? attack - 1 : 0;
        } else if (collect > 0) {
            s->ai.role = ORDER_COLLECT;
            collect--;
        } else if (steal > 0) {
            s->ai.role = ORDER_STEAL;
            steal--;
        } else if (attack > 0) {
            s->ai.role = ORDER_ATTACK;
            attack--;
        } else {
            s->ai.role = ORDER_GUARD;
        }
    }
}

void commander_update(GameState *gs) {
    for (i32 team = 0; team < MAX_TEAMS; team++) {
        const Team *t = &gs->teams[team];
        if (!t->ai || !t->active || t->eliminated || (gs->tick + (u64)team) % PERIOD != 0) {
            continue;
        }
        const Situation sit = look(gs, team);
        if (pad_usable(gs, team)) {
            build(gs, team, sit.squad, sit.pad_crates);
        }
        give_roles(gs, team, &sit);
    }
}
