#include "game/base.h"

#include "game/tuning.h"

#define ALERT_TICKS SECONDS_TO_TICKS(TUNING_BASE_ALERT_S)

void bases_init(GameState *gs) {
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        Team *team = &gs->teams[t];
        team->has_base = gs->level->has_spawn[t] && gs->level->pads[t].present;
        team->base_hp = team->has_base ? TUNING_BASE_HP : 0;
        team->base_hurt_ticks = 0;
    }
}

void bases_update(GameState *gs) {
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        if (gs->teams[t].base_hurt_ticks > 0) {
            gs->teams[t].base_hurt_ticks--;
        }
    }
}

bool base_alive(const GameState *gs, i32 team) {
    return team >= 0 && team < MAX_TEAMS && gs->teams[team].has_base && gs->teams[team].base_hp > 0;
}

Rect base_rect(const GameState *gs, i32 team) {
    if (team < 0 || team >= MAX_TEAMS || !gs->teams[team].has_base) {
        return (Rect){0, 0, 0, 0};
    }
    const Vec2 s = gs->teams[team].spawn;
    return (Rect){s.x - TUNING_BASE_W * 0.5f, s.y - TUNING_BASE_H, TUNING_BASE_W, TUNING_BASE_H};
}

i32 base_damage(GameState *gs, i32 team, i32 amount, i32 source_team) {
    if (amount <= 0 || team == source_team || !base_alive(gs, team)) {
        return 0;
    }
    Team *t = &gs->teams[team];
    const i32 done = MIN(amount, t->base_hp);
    t->base_hp -= done;
    const Rect r = base_rect(gs, team);
    const Vec2 c = vec2(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
    if (t->base_hurt_ticks == 0) {
        events_push(&gs->events, EV_BASE_ATTACKED, c, team, source_team);
    }
    t->base_hurt_ticks = ALERT_TICKS;
    if (source_team >= 0 && source_team < MAX_TEAMS) {
        gs->teams[source_team].stats.base_damage += (u32)done;
    }
    events_push(&gs->events, EV_DAMAGE, c, done, 2);
    if (t->base_hp == 0) {
        events_push(&gs->events, EV_BASE_DESTROYED, c, team, source_team);
    }
    return done;
}
