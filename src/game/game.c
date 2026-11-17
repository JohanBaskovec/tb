#include "game/game.h"

#include "core/log.h"
#include "game/crate.h"
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
    return true;
}

void game_update(GameState *gs, const InputFrame *input) {
    events_clear(&gs->events);
    companion_update(&gs->player, gs->level, input);
    crate_carry_update(gs, input);
    crates_update(gs);
    crate_carried_follow(gs);
    products_update(gs);

    if (companion_in_hazard(&gs->player, gs->level)) {
        crate_destroy(gs, gs->player.carrying); // the crate is lost with its carrier
        companion_init(&gs->player, gs->level->spawns[0]);
        gs->respawns++;
        events_push(&gs->events, EV_PLAYER_RESPAWNED, gs->player.body.pos, 0, 0);
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
    h = fnv_mix(h, gs->drop_timers, sizeof(gs->drop_timers));
    h = fnv_mix(h, &gs->products_built, sizeof(gs->products_built));
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
        }
    }
    return h;
}
