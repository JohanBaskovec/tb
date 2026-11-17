#include "game/game.h"

#include "core/log.h"

#include <string.h>

bool game_init(GameState *gs, u64 seed, const Level *level) {
    memset(gs, 0, sizeof(*gs));
    if (!level->has_spawn[0]) {
        LOG_ERROR("level '%s' has no spawn for team 1", level->name);
        return false;
    }
    rng_seed(&gs->rng, seed, 1);
    gs->level = level;
    companion_init(&gs->player, level->spawns[0]);
    return true;
}

void game_update(GameState *gs, const InputFrame *input) {
    companion_update(&gs->player, gs->level, input);

    if (companion_in_hazard(&gs->player, gs->level)) {
        companion_init(&gs->player, gs->level->spawns[0]);
        gs->respawns++;
    }
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
    return h;
}
