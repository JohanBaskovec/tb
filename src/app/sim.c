#include "app/sim.h"

#include "core/config.h"
#include "game/combat.h"
#include "game/goal.h"
#include "game/squad.h"
#include "game/tuning.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// The camera follows the controlled soldier; with nobody to control (all-AI match) the first one alive.
static Vec2 follow_target(const Sim *sim) {
    if (sim->gs.controlled < 0) {
        for (i32 i = 0; i < MAX_SOLDIERS; i++) {
            if (sim->gs.soldiers[i].alive) {
                return body_center(&sim->gs.soldiers[i].c.body);
            }
        }
    }
    return body_center(&game_player_const(&sim->gs)->body);
}

static void apply_ai_teams(Sim *sim) {
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        if ((sim->ai_teams & (1u << (u32)t)) != 0) {
            game_set_team_ai(&sim->gs, t, true);
        }
    }
    // Every team in the match gets its starting squad, side by side at its spawn.
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        if (!sim->gs.teams[t].playing) {
            continue;
        }
        const Vec2 spawn = sim->gs.teams[t].spawn;
        for (u32 k = 1; k < sim->start_squad; k++) {
            const f32 dx = (f32)((k + 1) / 2) * 14.0f * ((k % 2) ? 1.0f : -1.0f);
            if (soldier_spawn(&sim->gs, t, SOLDIER_NORMAL, vec2(spawn.x + dx, spawn.y)) < 0) {
                soldier_spawn_at_start(&sim->gs, t, SOLDIER_NORMAL, spawn);
            }
        }
    }
    events_clear(&sim->gs.events);
}

bool sim_init(Sim *sim, const SimConfig *cfg, char *err, usize err_len) {
    memset(sim, 0, sizeof(*sim));
    if (!arena_init(&sim->arena, 1u << 20)) {
        snprintf(err, err_len, "cannot allocate the level arena");
        return false;
    }
    bool ok;
    if (cfg->level_text) {
        ok = level_parse(&sim->level, &sim->arena, cfg->level_text, err, err_len);
    } else {
        ok = level_load_file(&sim->level, &sim->arena, cfg->level_path, err, err_len);
    }
    if (!ok) {
        arena_free(&sim->arena);
        return false;
    }
    if (!game_init(&sim->gs, cfg->seed, &sim->level)) {
        snprintf(err, err_len, "level has no spawn for team 1");
        arena_free(&sim->arena);
        return false;
    }
    sim->ai_teams = cfg->ai_teams;
    sim->start_squad = cfg->start_squad;
    if (cfg->ai_opponents) {
        for (i32 t = 1; t < MAX_TEAMS; t++) {
            if (sim->level.has_spawn[t] && sim->level.pads[t].present) {
                sim->ai_teams |= 1u << (u32)t;
            }
        }
    }
    apply_ai_teams(sim);
    sim->sfx.human_team = (sim->ai_teams & 1u) ? -1 : 0;
    fx_init(&sim->fx);
    if (cfg->script) {
        if (!script_parse(&sim->script, cfg->script, err, err_len)) {
            arena_free(&sim->arena);
            return false;
        }
        sim->use_script = true;
    }
    if (cfg->has_start) {
        companion_init(game_player(&sim->gs), cfg->start_feet);
    }
    sim->seed = cfg->seed;
    sim->has_start = cfg->has_start;
    sim->start_feet = cfg->start_feet;
    sim->use_bot = cfg->use_bot;
    bot_init(&sim->bot, cfg->seed);
    pad_menu_refresh(&sim->menu, &sim->gs, 0);
    camera_init(&sim->cam, (f32)LOGICAL_W, (f32)LOGICAL_H);
    camera_follow(&sim->cam, follow_target(sim), game_player_const(&sim->gs)->facing, &sim->level, true);
    return true;
}

void sim_restart(Sim *sim) {
    level_clear_dynamic(&sim->level);
    game_init(&sim->gs, sim->seed, &sim->level); // cannot fail: it worked once with this level
    apply_ai_teams(sim);
    if (sim->has_start) {
        companion_init(game_player(&sim->gs), sim->start_feet);
    }
    sim->menu = (PadMenu){0};
    fx_init(&sim->fx);
    sim->sfx = (SfxState){.human_team = (sim->ai_teams & 1u) ? -1 : 0};
    sim->cues = (SfxCueList){0};
    pad_menu_refresh(&sim->menu, &sim->gs, 0);
    camera_follow(&sim->cam, follow_target(sim), game_player_const(&sim->gs)->facing, &sim->level, true);
}

void sim_free(Sim *sim) {
    arena_free(&sim->arena);
}

bool sim_script_running(const Sim *sim) {
    return sim->use_script && !script_finished(&sim->script);
}

void sim_set_mouse(Sim *sim, Vec2 logical, bool left_down, bool right_down) {
    sim->mouse_active = true;
    sim->mouse_logical = logical;
    sim->mouse_left = left_down;
    if (right_down && !sim->mouse_right && sim->goal_menu.visible) {
        goal_menu_close(&sim->goal_menu); // a right click cancels the ring
        sfx_play_ui(&sim->sfx, SFX_MENU_BACK, &sim->cues);
    }
    sim->mouse_right = right_down;
    if (!left_down) {
        sim->mouse_fire_blocked = false;
    }
}

// Keyboard input as given, plus the mouse: aim at the pointer, fire while the left button is held.
static void sim_apply_human(Sim *sim, const InputFrame *human) {
    u32 down = human->down;
    sim->input.aim_active = human->aim_active; // an aim given with the keys (tests), unless the mouse takes over
    sim->input.aim = human->aim;
    if (sim->mouse_active) {
        const Vec2 world = sim_logical_to_world(sim, sim->mouse_logical);
        const bool on_menu = pad_menu_hit_test(&sim->menu, world) >= 0 || debug_menu_covers(&sim->debug, sim->mouse_logical) ||
                             sim->goal_menu.visible;
        sim->input.aim_active = true;
        sim->input.aim = world;
        if ((sim->mouse_left || sim->mouse_click_pending) && !sim->mouse_fire_blocked && !on_menu) {
            down |= action_bit(ACT_FIRE);
        }
        if (sim->mouse_right && !on_menu) {
            down |= action_bit(ACT_DESTROY);
        }
    }
    sim->mouse_click_pending = false;
    input_frame_update(&sim->input, down);
}

void sim_tick(Sim *sim, const InputFrame *human) {
    if (sim->use_script) {
        script_next(&sim->script, &sim->input);
    } else if (sim->use_bot) {
        bot_think(&sim->bot, &sim->input);
    } else if (human) {
        sim_apply_human(sim, human);
    } else {
        input_frame_update(&sim->input, 0);
    }

    // While the pad menu is up, Tab moves its selection: the game does not see it (it would also switch soldier).
    InputFrame game_input = sim->input;
    if (sim->menu.visible) {
        const u32 tab = action_bit(ACT_SWITCH);
        game_input.down &= ~tab;
        game_input.pressed &= ~tab;
        game_input.released &= ~tab;
    }
    pad_menu_handle_input(&sim->menu, &sim->gs, &sim->input);
    const bool menu_was_visible = sim->menu.visible;
    const i32 menu_selected = sim->menu.selected;
    const u32 respawns_before = sim->gs.respawns;
    const i32 controlled_before = sim->gs.controlled;
    game_update(&sim->gs, &game_input);
    pad_menu_refresh(&sim->menu, &sim->gs, 0);
    const Vec2 listener = vec2(sim->cam.pos.x + (f32)LOGICAL_W * 0.5f, sim->cam.pos.y + (f32)LOGICAL_H * 0.5f);
    sim->fx.focus = listener;
    fx_observe(&sim->fx, &sim->gs);
    fx_consume(&sim->fx, &sim->gs.events, &sim->gs); // the queue holds one tick only: drain it every tick
    fx_step(&sim->fx, &sim->level);
    sim->sfx.focus_soldier = sim->gs.controlled;
    sfx_consume(&sim->sfx, &sim->gs.events, listener, &sim->cues);
    if (sim->menu.visible && !menu_was_visible) {
        sfx_play_ui(&sim->sfx, SFX_PAD_OPEN, &sim->cues);
    } else if (sim->menu.visible && sim->menu.selected != menu_selected) {
        sfx_play_ui(&sim->sfx, SFX_MENU_MOVE, &sim->cues);
    }
    // A respawn is a jump; a change of controlled soldier is a smooth pan.
    const bool teleported = sim->gs.respawns != respawns_before && sim->gs.controlled == controlled_before;
    camera_follow(&sim->cam, follow_target(sim), game_player_const(&sim->gs)->facing, &sim->level, teleported);
}

Vec2 sim_logical_to_world(const Sim *sim, Vec2 logical) {
    return vec2(logical.x + roundf(sim->cam.pos.x), logical.y + roundf(sim->cam.pos.y));
}

void sim_toggle_debug_menu(Sim *sim) {
    sim->debug.visible = !sim->debug.visible;
}

bool sim_debug_spawn(Sim *sim, DebugItem item) {
    DebugFocus focus = {.facing = 1, .team = PLAYER_TEAM};
    if (sim->gs.controlled >= 0) {
        const Soldier *s = &sim->gs.soldiers[sim->gs.controlled];
        const Body *b = &s->c.body;
        focus = (DebugFocus){body_center(b), b->pos.y, b->pos.y + b->size.y, s->c.facing, s->team};
    } else {
        const Vec2 mid = vec2(roundf(sim->cam.pos.x) + (f32)LOGICAL_W * 0.5f, roundf(sim->cam.pos.y) + (f32)LOGICAL_H * 0.5f);
        focus.center = mid;
        focus.top = mid.y - TUNING_COMPANION_H * 0.5f;
        focus.bottom = mid.y + TUNING_COMPANION_H * 0.5f;
    }
    return debug_spawn(&sim->gs, item, &focus);
}

// The team the human plays for, or -1 when nobody does (spectating).
static i32 human_team(const Sim *sim) {
    return sim->gs.controlled >= 0 && !sim->gs.teams[sim->gs.soldiers[sim->gs.controlled].team].ai
               ? sim->gs.soldiers[sim->gs.controlled].team
               : -1;
}

void sim_toggle_goal_menu(Sim *sim, Vec2 logical) {
    if (sim->goal_menu.visible) {
        goal_menu_close(&sim->goal_menu);
        sfx_play_ui(&sim->sfx, SFX_MENU_BACK, &sim->cues);
        return;
    }
    const i32 team = human_team(sim);
    if (team < 0 || !sim->level.pads[team].present) {
        return;
    }
    goal_menu_open(&sim->goal_menu, logical);
    sim->mouse_fire_blocked = true; // the button that opened it must not fire when released over the game
    sfx_play_ui(&sim->sfx, SFX_PAD_OPEN, &sim->cues);
}

// A click while the ring is open.
static bool goal_menu_click(Sim *sim, Vec2 logical) {
    const i32 hit = goal_menu_hit(&sim->goal_menu, logical);
    const i32 team = human_team(sim);
    goal_menu_close(&sim->goal_menu);
    sim->mouse_fire_blocked = true; // this press belongs to the ring
    if (hit == GOAL_MENU_CENTER && team >= 0) {
        goal_clear(&sim->gs, team);
        sfx_play_ui(&sim->sfx, SFX_MENU_BACK, &sim->cues);
        return true;
    }
    if (hit >= 0 && team >= 0) {
        PadShape shape;
        bool blue;
        goal_menu_entry(hit, &shape, &blue);
        goal_set(&sim->gs, team, shape, blue);
        sfx_play_ui(&sim->sfx, SFX_MENU_OK, &sim->cues);
        return true;
    }
    sfx_play_ui(&sim->sfx, SFX_MENU_BACK, &sim->cues);
    return false;
}

bool sim_click(Sim *sim, Vec2 logical) {
    if (sim->goal_menu.visible) {
        return goal_menu_click(sim, logical);
    }
    if (debug_menu_covers(&sim->debug, logical)) {
        sim->mouse_fire_blocked = true; // the press belongs to the menu
        const i32 item = debug_menu_hit_test(&sim->debug, logical);
        return item >= 0 && sim_debug_spawn(sim, (DebugItem)item);
    }
    const Vec2 world = sim_logical_to_world(sim, logical);
    if (pad_menu_hit_test(&sim->menu, world) >= 0) {
        sim->mouse_fire_blocked = true; // this press belongs to the menu, not to the weapon
    } else {
        sim->mouse_click_pending = true; // a click shorter than a frame still fires
    }
    return pad_menu_click(&sim->menu, &sim->gs, world);
}
