#include "game/rules.h"

u32 rules_teams_playing(const GameState *gs) {
    u32 n = 0;
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        n += gs->teams[t].playing ? 1u : 0u;
    }
    return n;
}

u32 rules_teams_left(const GameState *gs) {
    u32 n = 0;
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        n += (gs->teams[t].playing && !gs->teams[t].eliminated) ? 1u : 0u;
    }
    return n;
}

void rules_update(GameState *gs) {
    if (gs->match.over || rules_teams_playing(gs) < 2 || rules_teams_left(gs) > 1) {
        return;
    }
    gs->match.over = true;
    gs->match.winner = -1;
    gs->match.end_tick = gs->tick;
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        if (gs->teams[t].playing && !gs->teams[t].eliminated) {
            gs->match.winner = t;
        }
    }
    events_push(&gs->events, EV_MATCH_OVER, vec2(0.0f, 0.0f), gs->match.winner, 0);
}
