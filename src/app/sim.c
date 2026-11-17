#include "app/sim.h"

#include "core/config.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static Vec2 follow_target(const Sim *sim) {
    return body_center(&sim->gs.player.body);
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
    if (cfg->script) {
        if (!script_parse(&sim->script, cfg->script, err, err_len)) {
            arena_free(&sim->arena);
            return false;
        }
        sim->use_script = true;
    }
    if (cfg->has_start) {
        companion_init(&sim->gs.player, cfg->start_feet);
    }
    sim->use_bot = cfg->use_bot;
    bot_init(&sim->bot, cfg->seed);
    pad_menu_refresh(&sim->menu, &sim->gs, 0);
    camera_init(&sim->cam, (f32)LOGICAL_W, (f32)LOGICAL_H);
    camera_follow(&sim->cam, follow_target(sim), sim->gs.player.facing, &sim->level, true);
    return true;
}

void sim_free(Sim *sim) {
    arena_free(&sim->arena);
}

bool sim_script_running(const Sim *sim) {
    return sim->use_script && !script_finished(&sim->script);
}

void sim_set_mouse(Sim *sim, Vec2 logical, bool left_down) {
    sim->mouse_active = true;
    sim->mouse_logical = logical;
    sim->mouse_left = left_down;
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
        const bool on_menu = pad_menu_hit_test(&sim->menu, world) >= 0;
        sim->input.aim_active = true;
        sim->input.aim = world;
        if ((sim->mouse_left || sim->mouse_click_pending) && !sim->mouse_fire_blocked && !on_menu) {
            down |= action_bit(ACT_FIRE);
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

    pad_menu_handle_input(&sim->menu, &sim->gs, &sim->input);

    const u32 respawns_before = sim->gs.respawns;
    game_update(&sim->gs, &sim->input);
    pad_menu_refresh(&sim->menu, &sim->gs, 0);
    fx_consume(&sim->fx, &sim->gs.events); // the queue holds one tick only: drain it every tick
    fx_step(&sim->fx);
    const bool teleported = sim->gs.respawns != respawns_before;
    camera_follow(&sim->cam, follow_target(sim), sim->gs.player.facing, &sim->level, teleported);
}

Vec2 sim_logical_to_world(const Sim *sim, Vec2 logical) {
    return vec2(logical.x + roundf(sim->cam.pos.x), logical.y + roundf(sim->cam.pos.y));
}

bool sim_click(Sim *sim, Vec2 logical) {
    const Vec2 world = sim_logical_to_world(sim, logical);
    if (pad_menu_hit_test(&sim->menu, world) >= 0) {
        sim->mouse_fire_blocked = true; // this press belongs to the menu, not to the weapon
    } else {
        sim->mouse_click_pending = true; // a click shorter than a frame still fires
    }
    return pad_menu_click(&sim->menu, &sim->gs, world);
}
