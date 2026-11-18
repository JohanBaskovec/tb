// Bases: the building of every team that has a spawn and a pad. No SDL.
//
// A base stands on its team's spawn (soldiers walk in front of it: it is not solid). It has
// TUNING_BASE_HP hit points and takes damage from every weapon of the other teams: bullets and punches
// stop on it, explosions reach it (not those of its own team). A destroyed base stays as a ruin: its pad
// cannot be used any more, but the team keeps playing with the soldiers it has left.
#pragma once

#include "game/game.h"

// Gives a base to every team with a spawn and a pad (game_init).
void bases_init(GameState *gs);
// Timers. Once per tick.
void bases_update(GameState *gs);
// True if the team has a base that is still standing.
bool base_alive(const GameState *gs, i32 team);
// Rectangle of the building (the ruin, once destroyed); zero size if the team has no base.
Rect base_rect(const GameState *gs, i32 team);
// Takes hit points from the base of `team`, credited to `source_team` (-1: nobody). Returns the damage
// actually done (0 if there is no standing base, or it belongs to `source_team`).
i32 base_damage(GameState *gs, i32 team, i32 amount, i32 source_team);
