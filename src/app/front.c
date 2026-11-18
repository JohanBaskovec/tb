#include "app/front.h"

#include "core/config.h"
#include "game/tuning.h"

#include <stdio.h>
#include <string.h>

const FrontLevel FRONT_LEVELS[FRONT_LEVEL_COUNT] = {
    {"beach.lvl", "LA CRIQUE"},
    {"ai_arena.lvl", "L'ARÈNE"},
    {"test_arena.lvl", "TERRAIN D'ESSAI"},
};

#define RESULTS_DELAY SECONDS_TO_TICKS(TUNING_RESULTS_DELAY_S)

static void menu_keys(Front *f, const InputFrame *in, i32 count);

static void open_screen(Front *f, Screen screen) {
    f->screen = screen;
    f->selected = 0;
    f->screen_ticks = 0;
}

static void load_preview(Front *f) {
    char path[768];
    char err[128];
    snprintf(path, sizeof(path), "%s/levels/%s", f->assets, FRONT_LEVELS[f->level_index].file);
    arena_reset(&f->preview_arena);
    f->preview_ok = level_load_file(&f->preview, &f->preview_arena, path, err, sizeof(err));
}

static void end_match(Front *f) {
    if (f->sim_ready) {
        sim_free(&f->sim);
        f->sim_ready = false;
    }
}

static bool start_match(Front *f) {
    end_match(f);
    char path[768];
    snprintf(path, sizeof(path), "%s/levels/%s", f->assets, FRONT_LEVELS[f->level_index].file);
    const SimConfig cfg = {.level_path = path,
                           .seed = f->seed,
                           .ai_teams = f->spectate ? 1u : 0u,
                           .ai_opponents = true,
                           .start_squad = TUNING_SKIRMISH_START_SQUAD};
    f->error[0] = '\0';
    if (!sim_init(&f->sim, &cfg, f->error, sizeof(f->error))) {
        return false;
    }
    f->sim_ready = true;
    f->seed++;
    f->over_ticks = 0;
    open_screen(f, SCREEN_PLAY);
    return true;
}

bool front_init(Front *f, const FrontConfig *cfg) {
    memset(f, 0, sizeof(*f));
    snprintf(f->assets, sizeof(f->assets), "%s", cfg->assets_dir);
    f->seed = cfg->seed;
    if (!arena_init(&f->preview_arena, 1u << 20)) {
        return false;
    }
    open_screen(f, SCREEN_TITLE);
    load_preview(f);
    return true;
}

void front_free(Front *f) {
    end_match(f);
    arena_free(&f->preview_arena);
}

Sim *front_sim(Front *f) {
    return f->sim_ready && f->screen != SCREEN_TITLE && f->screen != SCREEN_SETUP ? &f->sim : NULL;
}

const Sim *front_sim_const(const Front *f) {
    return f->sim_ready && f->screen != SCREEN_TITLE && f->screen != SCREEN_SETUP ? &f->sim : NULL;
}

// ---- Layout ------------------------------------------------------------------------------------

i32 front_menu_count(const Front *f) {
    switch (f->screen) {
    case SCREEN_TITLE:
        return TITLE_COUNT;
    case SCREEN_SETUP:
        return SETUP_COUNT;
    case SCREEN_PAUSE:
        return PAUSE_COUNT;
    case SCREEN_RESULTS:
        return RESULTS_COUNT;
    default:
        return 0;
    }
}

Rect front_menu_rect(const Front *f, i32 index) {
    const f32 cx = (f32)LOGICAL_W * 0.5f;
    switch (f->screen) {
    case SCREEN_TITLE:
        return (Rect){cx - 70.0f, 236.0f + (f32)index * 24.0f, 140.0f, 18.0f};
    case SCREEN_SETUP:
        return (Rect){cx - 150.0f, 210.0f + (f32)index * 24.0f, 300.0f, 18.0f};
    case SCREEN_PAUSE:
        return (Rect){cx - 80.0f, 150.0f + (f32)index * 24.0f, 160.0f, 18.0f};
    case SCREEN_RESULTS:
        return (Rect){cx - 150.0f + (f32)index * 160.0f, 300.0f, 140.0f, 18.0f};
    default:
        return (Rect){0, 0, 0, 0};
    }
}

static i32 entry_at(const Front *f, Vec2 p) {
    for (i32 i = 0; i < front_menu_count(f); i++) {
        const Rect r = front_menu_rect(f, i);
        if (p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h) {
            return i;
        }
    }
    return -1;
}

// ---- Menus -------------------------------------------------------------------------------------

// A setting changes by `step` (left -1, right / confirm +1).
static void change_setting(Front *f, i32 step) {
    if (f->screen != SCREEN_SETUP) {
        return;
    }
    if (f->selected == SETUP_LEVEL) {
        f->level_index = (f->level_index + step + FRONT_LEVEL_COUNT) % FRONT_LEVEL_COUNT;
        f->error[0] = '\0';
        load_preview(f);
    } else if (f->selected == SETUP_MODE) {
        f->spectate = !f->spectate;
    }
}

// The selected entry is validated.
static void activate(Front *f) {
    switch (f->screen) {
    case SCREEN_TITLE:
        if (f->selected == TITLE_PLAY) {
            open_screen(f, SCREEN_SETUP);
            f->selected = SETUP_START;
        } else {
            open_screen(f, SCREEN_QUIT);
        }
        break;
    case SCREEN_SETUP:
        if (f->selected == SETUP_START) {
            start_match(f); // on failure the setup screen stays, with the error
        } else if (f->selected == SETUP_BACK) {
            open_screen(f, SCREEN_TITLE);
        } else {
            change_setting(f, 1);
        }
        break;
    case SCREEN_PAUSE:
        if (f->selected == PAUSE_RESUME) {
            open_screen(f, SCREEN_PLAY);
        } else if (f->selected == PAUSE_RESTART) {
            sim_restart(&f->sim);
            f->over_ticks = 0;
            open_screen(f, SCREEN_PLAY);
        } else {
            end_match(f);
            open_screen(f, SCREEN_TITLE);
        }
        break;
    case SCREEN_RESULTS:
        if (f->selected == RESULTS_AGAIN) {
            if (!start_match(f)) {
                open_screen(f, SCREEN_SETUP);
            }
        } else {
            end_match(f);
            open_screen(f, SCREEN_TITLE);
        }
        break;
    default:
        break;
    }
}

static void back(Front *f) {
    switch (f->screen) {
    case SCREEN_SETUP:
        open_screen(f, SCREEN_TITLE);
        break;
    case SCREEN_PAUSE:
        open_screen(f, SCREEN_PLAY);
        break;
    case SCREEN_RESULTS:
        end_match(f);
        open_screen(f, SCREEN_TITLE);
        break;
    default:
        break;
    }
}

static void beep(Front *f, SfxId id) {
    sfx_push(&f->cues, id, 1.0f, 0.0f);
}

static void menu_tick(Front *f, const InputFrame *in) {
    const i32 count = front_menu_count(f);
    const i32 before = f->selected, level = f->level_index;
    const bool spectate = f->spectate;
    const Screen screen = f->screen;
    if (input_pressed(in, ACT_CONFIRM) || input_pressed(in, ACT_JUMP)) {
        beep(f, SFX_MENU_OK);
    } else if (input_pressed(in, ACT_PAUSE) && f->screen != SCREEN_TITLE) {
        beep(f, SFX_MENU_BACK);
    }
    menu_keys(f, in, count);
    if (f->screen == screen && (f->selected != before || f->level_index != level || f->spectate != spectate)) {
        beep(f, SFX_MENU_MOVE);
    }
}

static void menu_keys(Front *f, const InputFrame *in, i32 count) {
    if (input_pressed(in, ACT_UP)) {
        f->selected = (f->selected + count - 1) % count;
    } else if (input_pressed(in, ACT_DOWN)) {
        f->selected = (f->selected + 1) % count;
    } else if (input_pressed(in, ACT_LEFT)) {
        if (f->screen == SCREEN_RESULTS) {
            f->selected = (f->selected + count - 1) % count; // a row of buttons
        } else {
            change_setting(f, -1);
        }
    } else if (input_pressed(in, ACT_RIGHT)) {
        if (f->screen == SCREEN_RESULTS) {
            f->selected = (f->selected + 1) % count;
        } else {
            change_setting(f, 1);
        }
    } else if (input_pressed(in, ACT_CONFIRM) || input_pressed(in, ACT_JUMP)) {
        activate(f);
    } else if (input_pressed(in, ACT_PAUSE)) {
        back(f);
    }
}

static void play_tick(Front *f, const InputFrame *in) {
    if (input_pressed(in, ACT_PAUSE)) {
        beep(f, SFX_MENU_OK);
        open_screen(f, SCREEN_PAUSE);
        return;
    }
    sim_tick(&f->sim, in);
    if (f->sim.gs.match.over && ++f->over_ticks >= RESULTS_DELAY) {
        open_screen(f, SCREEN_RESULTS);
    }
}

void front_tick(Front *f, const InputFrame *in) {
    f->screen_ticks++;
    if (f->screen == SCREEN_PLAY) {
        play_tick(f, in);
    } else if (f->screen != SCREEN_QUIT) {
        menu_tick(f, in);
    }
}

void front_point(Front *f, Vec2 logical) {
    const i32 i = entry_at(f, logical);
    if (i >= 0) {
        f->selected = i;
    }
}

bool front_click(Front *f, Vec2 logical) {
    if (f->screen == SCREEN_PLAY) {
        sim_click(&f->sim, logical);
        return false;
    }
    const i32 i = entry_at(f, logical);
    if (i < 0) {
        return false;
    }
    f->selected = i;
    beep(f, SFX_MENU_OK);
    activate(f);
    return true;
}
