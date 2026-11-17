#include "app/sim.h"

#include "core/config.h"

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

void sim_tick(Sim *sim, const InputFrame *human) {
    if (sim->use_script) {
        script_next(&sim->script, &sim->input);
    } else if (sim->use_bot) {
        bot_think(&sim->bot, &sim->input);
    } else if (human) {
        sim->input = *human;
    } else {
        input_frame_update(&sim->input, 0);
    }

    const u32 respawns_before = sim->gs.respawns;
    game_update(&sim->gs, &sim->input);
    const bool teleported = sim->gs.respawns != respawns_before;
    camera_follow(&sim->cam, follow_target(sim), sim->gs.player.facing, &sim->level, teleported);
}
