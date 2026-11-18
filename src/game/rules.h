// Skirmish rules: who is still in the match, and when it is over. No SDL.
//
// A team takes part in the match once it has had a soldier (`Team.playing`). It is eliminated when its last
// soldier dies: there is no respawn, and a team without soldiers cannot use its pad any more. The match is over
// when at most one of the teams taking part is left, provided at least two took part (a level played alone,
// against dummies, never ends). The last team standing wins; if the last ones fall together it is a draw.
#pragma once

#include "game/game.h"

// Checks the end of the match (end of game_update). The result is latched: once over, it stays over.
void rules_update(GameState *gs);
// Number of teams taking part / still in the match.
u32 rules_teams_playing(const GameState *gs);
u32 rules_teams_left(const GameState *gs);
