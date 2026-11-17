#include "game/game.h"

#include "core/log.h"
#include "game/combat.h"
#include "game/crate.h"
#include "game/loot.h"
#include "game/pad.h"

#include <string.h>

bool game_init(GameState *gs, u64 seed, Level *level) {
    memset(gs, 0, sizeof(*gs));
    if (!level->has_spawn[0]) {
        LOG_ERROR("level '%s' has no spawn for team 1", level->name);
        return false;
    }
    rng_seed(&gs->rng, seed, 1);
    gs->level = level;
    pool_init(&gs->entities);
    companion_init(&gs->player, level->spawns[0]);
    for (i32 z = 0; z < level->drop_zone_count; z++) {
        gs->drop_timers[z] = (u32)level->drop_cfg[z].interval_s * TICK_HZ;
    }
    for (i32 d = 0; d < level->dummy_count; d++) {
        dummy_spawn(gs, level->dummies[d], 1);
    }
    return true;
}

static u32 crate_actions(const GameState *gs) {
    return gs->crate_stats.picked_up + gs->crate_stats.placed + gs->crate_stats.thrown;
}

static void respawn_player(GameState *gs) {
    companion_init(&gs->player, gs->level->spawns[0]);
    gs->respawns++;
    events_push(&gs->events, EV_PLAYER_RESPAWNED, gs->player.body.pos, 0, 0);
}

void game_update(GameState *gs, const InputFrame *input) {
    events_clear(&gs->events);
    const bool was_carrying = entity_id_valid(gs->player.carrying);
    const u32 crate_actions_before = crate_actions(gs);
    companion_update(&gs->player, gs->level, input);
    crate_carry_update(gs, input);
    // A key a crate action used is not also a shot or a weapon pickup.
    combat_player_update(gs, input, was_carrying || crate_actions(gs) != crate_actions_before);
    combat_update(gs);
    crates_update(gs);
    crate_carried_follow(gs);
    loot_update(gs);

    if (gs->player.hp <= 0) {
        combat_player_died(gs); // drops the weapon and the crate where the soldier fell
        respawn_player(gs);
    } else if (companion_in_hazard(&gs->player, gs->level)) {
        crate_destroy(gs, gs->player.carrying); // the crate is lost with its carrier (and so is the weapon)
        respawn_player(gs);
    }
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

u64 game_hash(const GameState *gs) {
    // Field by field: struct padding bytes are indeterminate.
    const Companion *c = &gs->player;
    u64 h = 14695981039346656037ULL;
    h = fnv_mix(h, &gs->tick, sizeof(gs->tick));
    h = fnv_mix(h, &gs->rng.state, sizeof(gs->rng.state));
    h = fnv_mix(h, &gs->respawns, sizeof(gs->respawns));
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
