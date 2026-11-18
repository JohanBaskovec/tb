#include "game/game.h"

#include "core/log.h"
#include "game/combat.h"
#include "game/commander.h"
#include "game/crate.h"
#include "game/loot.h"
#include "game/pad.h"
#include "game/squad.h"

#include <string.h>

bool game_init(GameState *gs, u64 seed, Level *level) {
    memset(gs, 0, sizeof(*gs));
    if (!level->has_spawn[0]) {
        LOG_ERROR("level '%s' has no spawn for team 1", level->name);
        return false;
    }
    rng_seed(&gs->rng, seed, 1);
    gs->level = level;
    gs->controlled = -1;
    pool_init(&gs->entities);
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        gs->teams[t].active = level->has_spawn[t];
        gs->teams[t].spawn = level->spawns[t];
        gs->teams[t].order = t == PLAYER_TEAM ? ORDER_FOLLOW : ORDER_GUARD;
    }
    for (i32 z = 0; z < level->drop_zone_count; z++) {
        gs->drop_timers[z] = (u32)level->drop_cfg[z].interval_s * TICK_HZ;
    }
    for (i32 d = 0; d < level->dummy_count; d++) {
        dummy_spawn(gs, level->dummies[d], TEAM_NONE);
    }
    // The human starts with one soldier. Other teams get soldiers from their pads.
    soldier_spawn_at_start(gs, PLAYER_TEAM, SOLDIER_NORMAL, level->spawns[PLAYER_TEAM]);
    events_clear(&gs->events);
    return true;
}

void game_set_team_ai(GameState *gs, i32 team, bool ai) {
    if (team < 0 || team >= MAX_TEAMS) {
        return;
    }
    gs->teams[team].ai = ai;
    if (ai && !gs->nav.built) {
        nav_build(&gs->nav, gs->level); // now, not in the middle of a tick
    }
    if (ai && gs->controlled >= 0 && gs->soldiers[gs->controlled].team == team) {
        gs->ghost = gs->soldiers[gs->controlled].c;
        gs->ghost.hp = 0;
        gs->controlled = -1; // nobody is behind this team any more
    }
    if (ai && gs->teams[team].active && team_soldier_count(gs, team) == 0) {
        soldier_spawn_at_start(gs, team, SOLDIER_NORMAL, gs->teams[team].spawn);
    }
}

void game_update(GameState *gs, const InputFrame *input) {
    events_clear(&gs->events);
    if (input_pressed(input, ACT_SWITCH)) {
        squad_cycle_control(gs);
    }
    if (input_pressed(input, ACT_ORDER)) {
        squad_cycle_order(gs);
    }
    commander_update(gs);
    squad_update(gs, input);
    combat_update(gs);
    crates_update(gs);
    crate_carried_follow(gs);
    loot_update(gs);
    squad_resolve(gs);
    pool_flush(&gs->entities);
    gs->tick++;
}

static u64 fnv_mix(u64 h, const void *data, usize len) {
    const u8 *p = (const u8 *)data;
    for (usize i = 0; i < len; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static u64 hash_companion(u64 h, const Companion *c) {
    h = fnv_mix(h, &c->body.pos, sizeof(c->body.pos));
    h = fnv_mix(h, &c->body.vel, sizeof(c->body.vel));
    h = fnv_mix(h, &c->facing, sizeof(c->facing));
    u32 flags = (u32)c->body.on_ground | ((u32)c->on_ladder << 1) | ((u32)c->is_jumping << 2);
    h = fnv_mix(h, &flags, sizeof(flags));
    h = fnv_mix(h, &c->coyote_ticks, sizeof(c->coyote_ticks));
    h = fnv_mix(h, &c->jump_buffer_ticks, sizeof(c->jump_buffer_ticks));
    h = fnv_mix(h, &c->drop_ticks, sizeof(c->drop_ticks));
    h = fnv_mix(h, &c->ladder_cooldown_ticks, sizeof(c->ladder_cooldown_ticks));
    h = fnv_mix(h, &c->carrying, sizeof(c->carrying));
    h = fnv_mix(h, &c->aim_dir, sizeof(c->aim_dir));
    h = fnv_mix(h, &c->hp, sizeof(c->hp));
    h = fnv_mix(h, &c->weapon, sizeof(c->weapon));
    h = fnv_mix(h, &c->ammo, sizeof(c->ammo));
    h = fnv_mix(h, &c->fire_cooldown, sizeof(c->fire_cooldown));
    h = fnv_mix(h, &c->hurt_ticks, sizeof(c->hurt_ticks));
    h = fnv_mix(h, &c->punch_ticks, sizeof(c->punch_ticks));
    return h;
}

static u64 hash_nav_agent(u64 h, const NavAgent *a) {
    h = fnv_mix(h, &a->walker.target_x, sizeof(a->walker.target_x));
    h = fnv_mix(h, &a->walker.jump_hold, sizeof(a->walker.jump_hold));
    h = fnv_mix(h, &a->walker.blocked_ticks, sizeof(a->walker.blocked_ticks));
    h = fnv_mix(h, &a->goal1, sizeof(a->goal1));
    h = fnv_mix(h, &a->last_node1, sizeof(a->last_node1));
    h = fnv_mix(h, a->path, sizeof(a->path));
    h = fnv_mix(h, &a->path_len, sizeof(a->path_len));
    h = fnv_mix(h, &a->path_pos, sizeof(a->path_pos));
    const u32 flags = (u32)a->in_macro | ((u32)a->airborne << 1) | ((u32)a->fails << 2);
    h = fnv_mix(h, &flags, sizeof(flags));
    h = fnv_mix(h, &a->macro_tick, sizeof(a->macro_tick));
    h = fnv_mix(h, &a->progress, sizeof(a->progress));
    h = fnv_mix(h, &a->banned1, sizeof(a->banned1));
    h = fnv_mix(h, &a->banned_ticks, sizeof(a->banned_ticks));
    h = fnv_mix(h, &a->fail_ticks, sizeof(a->fail_ticks));
    return h;
}

u64 game_hash(const GameState *gs) {
    // Field by field: struct padding bytes are indeterminate.
    u64 h = 14695981039346656037ULL;
    h = fnv_mix(h, &gs->tick, sizeof(gs->tick));
    h = fnv_mix(h, &gs->rng.state, sizeof(gs->rng.state));
    h = fnv_mix(h, &gs->respawns, sizeof(gs->respawns));
    h = fnv_mix(h, &gs->controlled, sizeof(gs->controlled));
    h = hash_companion(h, game_player_const(gs));
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        const u32 flags = (u32)gs->teams[t].eliminated | ((u32)gs->teams[t].order << 1) | ((u32)gs->teams[t].ai << 5);
        h = fnv_mix(h, &flags, sizeof(flags));
        h = fnv_mix(h, &gs->teams[t].stagnant_ticks, sizeof(u32));
        h = fnv_mix(h, &gs->teams[t].pad_crates, sizeof(u32));
    }
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        const Soldier *s = &gs->soldiers[i];
        if (!s->alive) {
            continue;
        }
        h = fnv_mix(h, &i, sizeof(i));
        h = fnv_mix(h, &s->team, sizeof(s->team));
        h = fnv_mix(h, &s->variant, sizeof(s->variant));
        h = hash_companion(h, &s->c);
        h = fnv_mix(h, &s->ai.walker.target_x, sizeof(s->ai.walker.target_x));
        h = fnv_mix(h, &s->ai.walker.jump_hold, sizeof(s->ai.walker.jump_hold));
        h = fnv_mix(h, &s->ai.walker.blocked_ticks, sizeof(s->ai.walker.blocked_ticks));
        h = fnv_mix(h, &s->ai.clock, sizeof(s->ai.clock));
        h = fnv_mix(h, &s->ai.collect, sizeof(s->ai.collect));
        h = fnv_mix(h, &s->ai.deliver_dir, sizeof(s->ai.deliver_dir));
        h = fnv_mix(h, &s->ai.crate, sizeof(s->ai.crate));
        h = fnv_mix(h, &s->ai.state_ticks, sizeof(s->ai.state_ticks));
        h = fnv_mix(h, s->ai.banned, sizeof(s->ai.banned));
        h = fnv_mix(h, s->ai.banned_ticks, sizeof(s->ai.banned_ticks));
        h = fnv_mix(h, &s->ai.role, sizeof(s->ai.role));
        h = fnv_mix(h, &s->ai.mode, sizeof(s->ai.mode));
        h = hash_nav_agent(h, &s->ai.nav);
    }
    h = fnv_mix(h, gs->drop_timers, sizeof(gs->drop_timers));
    h = fnv_mix(h, &gs->products_built, sizeof(gs->products_built));
    h = fnv_mix(h, &gs->combat_stats.shots, sizeof(u32));
    h = fnv_mix(h, &gs->combat_stats.hits, sizeof(u32));
    h = fnv_mix(h, &gs->combat_stats.explosions, sizeof(u32));
    h = fnv_mix(h, &gs->combat_stats.kills, sizeof(u32));
    h = fnv_mix(h, &gs->combat_stats.deaths, sizeof(u32));
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type == ENT_NONE) {
            continue;
        }
        h = fnv_mix(h, &i, sizeof(i));
        h = fnv_mix(h, &e->generation, sizeof(e->generation));
        h = fnv_mix(h, &e->body.pos, sizeof(e->body.pos));
        h = fnv_mix(h, &e->body.vel, sizeof(e->body.vel));
        if (e->type == ENT_CRATE) {
            const Crate *cr = &e->as.crate;
            h = fnv_mix(h, &cr->kind, sizeof(cr->kind));
            h = fnv_mix(h, &cr->state, sizeof(cr->state));
            h = fnv_mix(h, &cr->cell_x, sizeof(cr->cell_x));
            h = fnv_mix(h, &cr->cell_y, sizeof(cr->cell_y));
            h = fnv_mix(h, &cr->origin_zone, sizeof(cr->origin_zone));
        } else if (e->type == ENT_PRODUCT) {
            h = fnv_mix(h, &e->as.product.kind, sizeof(e->as.product.kind));
            h = fnv_mix(h, &e->as.product.team, sizeof(e->as.product.team));
        } else if (e->type == ENT_WEAPON) {
            h = fnv_mix(h, &e->as.weapon.kind, sizeof(e->as.weapon.kind));
            h = fnv_mix(h, &e->as.weapon.ammo, sizeof(e->as.weapon.ammo));
        } else if (e->type == ENT_PROJECTILE) {
            h = fnv_mix(h, &e->as.projectile.weapon, sizeof(e->as.projectile.weapon));
            h = fnv_mix(h, &e->as.projectile.team, sizeof(e->as.projectile.team));
            h = fnv_mix(h, &e->as.projectile.ttl, sizeof(e->as.projectile.ttl));
        } else if (e->type == ENT_DUMMY) {
            h = fnv_mix(h, &e->as.dummy.hp, sizeof(e->as.dummy.hp));
            h = fnv_mix(h, &e->as.dummy.hurt_ticks, sizeof(e->as.dummy.hurt_ticks));
        }
    }
    return h;
}
