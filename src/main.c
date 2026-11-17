#include "app/sim.h"
#include "core/config.h"
#include "core/log.h"
#include "game/combat.h"
#include "game/crate.h"
#include "game/pad.h"
#include "game/squad.h"
#include "game/walker.h"
#include "platform/platform.h"
#include "render/draw_game.h"
#include "render/render.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CS_ASSET_DIR
#define CS_ASSET_DIR "assets"
#endif

typedef struct Options {
    bool headless;
    bool bot;
    bool frames_set;
    u32 frames;     // headless: ticks to simulate
    u32 max_ticks;  // windowed: quit after this many ticks (0 = run until closed)
    u64 seed;
    const char *assets;
    const char *level;
    const char *script;
    const char *shot;     // PNG path (or pattern with %u when shot_every > 0)
    u32 shot_every;
    bool has_shot_cam;
    Vec2 shot_cam;
    bool debug_overlay;
    bool has_start;
    Vec2 start_feet;
    bool has_walk_to;
    f32 walk_to;
    u32 crate_count;
    Vec2 crate_pos[16];
    CrateKind crate_kind[16];
    bool has_fill_pad;
    bool has_order;
    Order order;
    u32 soldier_count;
    i32 soldier_team[16];
    SoldierVariant soldier_variant[16];
    f32 soldier_x[16];
    bool has_give;
    WeaponKind give;
    char fill_top[8], fill_bottom[8];
    u32 trace_every;
    const char *expect_driver;
} Options;

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  --headless              no window: simulate and exit (needs no display)\n"
            "  --frames N              headless: ticks to simulate (default 600, or the script length)\n"
            "  --max-ticks N           windowed: quit after N ticks\n"
            "  --bot                   drive the player with a random bot\n"
            "  --script \"R:60 RJ:12\"   drive the player with an input script (see game/script.h)\n"
            "  --seed N                RNG seed (default 1)\n"
            "  --level PATH            level file (default <assets>/levels/test_arena.lvl)\n"
            "  --assets DIR            assets directory\n"
            "  --screenshot PATH       write a PNG after the last tick\n"
            "  --shot-every N          with --screenshot PATTERN (containing %%u): one PNG every N ticks\n"
            "  --shot-cam X,Y          screenshot camera top-left, in pixels\n"
            "  --start X,Y             start with the feet at X,Y (pixels) instead of the spawn\n"
            "  --walk-to X             drive the player with a bot that walks to x=X and hops obstacles\n"
            "  --crate X,Y,KIND        add a falling crate (top-left X,Y in pixels; KIND red or blue); repeatable\n"
            "  --fill-pad TOP,BOTTOM   put crates in the team-1 pad, e.g. rb..,rrr. (r red, b blue, . empty)\n"
            "  --soldier TEAM,VARIANT,X add a soldier (TEAM 1-4; VARIANT normal|strong|heavy|armored; feet x in pixels, at the\n"
            "                          height of the team-1 spawn); repeatable\n"
            "  --order ORDER           order of team 1: follow, guard, collect or attack\n"
            "  --give WEAPON           start with a full weapon: pistol, uzi, grenades, strong-grenades, rocket\n"
            "  --debug-overlay         draw hitboxes, grid and state flags\n"
            "  --trace N               print the player state every N ticks\n"
            "  --expect-video-driver D abort unless SDL picked video driver D (e.g. dummy)\n",
            prog);
}

static bool parse_args(int argc, char **argv, Options *o) {
    memset(o, 0, sizeof(*o));
    o->frames = 600;
    o->seed = 1;
    o->assets = CS_ASSET_DIR;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const bool has_val = i + 1 < argc;
        if (strcmp(a, "--headless") == 0) {
            o->headless = true;
        } else if (strcmp(a, "--bot") == 0) {
            o->bot = true;
        } else if (strcmp(a, "--debug-overlay") == 0) {
            o->debug_overlay = true;
        } else if (strcmp(a, "--frames") == 0 && has_val) {
            o->frames = (u32)strtoul(argv[++i], NULL, 10);
            o->frames_set = true;
        } else if (strcmp(a, "--max-ticks") == 0 && has_val) {
            o->max_ticks = (u32)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(a, "--seed") == 0 && has_val) {
            o->seed = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(a, "--assets") == 0 && has_val) {
            o->assets = argv[++i];
        } else if (strcmp(a, "--level") == 0 && has_val) {
            o->level = argv[++i];
        } else if (strcmp(a, "--script") == 0 && has_val) {
            o->script = argv[++i];
        } else if (strcmp(a, "--screenshot") == 0 && has_val) {
            o->shot = argv[++i];
        } else if (strcmp(a, "--shot-every") == 0 && has_val) {
            o->shot_every = (u32)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(a, "--shot-cam") == 0 && has_val) {
            float x, y;
            if (sscanf(argv[++i], "%f,%f", &x, &y) != 2) {
                LOG_ERROR("--shot-cam expects X,Y");
                return false;
            }
            o->has_shot_cam = true;
            o->shot_cam = vec2(x, y);
        } else if (strcmp(a, "--start") == 0 && has_val) {
            float x, y;
            if (sscanf(argv[++i], "%f,%f", &x, &y) != 2) {
                LOG_ERROR("--start expects X,Y");
                return false;
            }
            o->has_start = true;
            o->start_feet = vec2(x, y);
        } else if (strcmp(a, "--crate") == 0 && has_val) {
            float x, y;
            char kind[8];
            if (o->crate_count >= 16 || sscanf(argv[++i], "%f,%f,%7s", &x, &y, kind) != 3 ||
                (strcmp(kind, "red") != 0 && strcmp(kind, "blue") != 0)) {
                LOG_ERROR("--crate expects X,Y,red|blue (at most 16)");
                return false;
            }
            o->crate_pos[o->crate_count] = vec2(x, y);
            o->crate_kind[o->crate_count] = strcmp(kind, "blue") == 0 ? CRATE_BLUE : CRATE_RED;
            o->crate_count++;
        } else if (strcmp(a, "--fill-pad") == 0 && has_val) {
            if (sscanf(argv[++i], "%4[.rb],%4[.rb]", o->fill_top, o->fill_bottom) != 2 ||
                strlen(o->fill_top) != 4 || strlen(o->fill_bottom) != 4) {
                LOG_ERROR("--fill-pad expects two rows of 4 characters from . r b, e.g. rb..,rrr.");
                return false;
            }
            o->has_fill_pad = true;
        } else if (strcmp(a, "--soldier") == 0 && has_val) {
            int team;
            float x;
            char variant[16];
            const char *spec = argv[++i];
            if (o->soldier_count >= 16 || sscanf(spec, "%d,%15[a-z],%f", &team, variant, &x) != 3 || team < 1 ||
                team > MAX_TEAMS) {
                LOG_ERROR("--soldier expects TEAM,VARIANT,X (at most 16)");
                return false;
            }
            SoldierVariant v;
            if (strcmp(variant, "normal") == 0) {
                v = SOLDIER_NORMAL;
            } else if (strcmp(variant, "strong") == 0) {
                v = SOLDIER_STRONG;
            } else if (strcmp(variant, "heavy") == 0) {
                v = SOLDIER_HEAVY;
            } else if (strcmp(variant, "armored") == 0) {
                v = SOLDIER_HEAVY_ARMORED;
            } else {
                LOG_ERROR("--soldier: unknown variant '%s'", variant);
                return false;
            }
            o->soldier_team[o->soldier_count] = team - 1;
            o->soldier_variant[o->soldier_count] = v;
            o->soldier_x[o->soldier_count] = x;
            o->soldier_count++;
        } else if (strcmp(a, "--order") == 0 && has_val) {
            const char *w = argv[++i];
            o->has_order = true;
            Order found = ORDER_COUNT;
            for (int k = 0; k < ORDER_COUNT; k++) {
                if (strcmp(w, order_name((Order)k)) == 0) {
                    found = (Order)k;
                }
            }
            if (found == ORDER_COUNT) {
                LOG_ERROR("--order expects follow, guard, collect or attack");
                return false;
            }
            o->order = found;
        } else if (strcmp(a, "--give") == 0 && has_val) {
            const char *w = argv[++i];
            o->has_give = true;
            if (strcmp(w, "pistol") == 0) {
                o->give = WEAPON_PISTOL;
            } else if (strcmp(w, "uzi") == 0) {
                o->give = WEAPON_UZI;
            } else if (strcmp(w, "grenades") == 0) {
                o->give = WEAPON_GRENADES;
            } else if (strcmp(w, "strong-grenades") == 0) {
                o->give = WEAPON_STRONG_GRENADES;
            } else if (strcmp(w, "rocket") == 0) {
                o->give = WEAPON_ROCKET_LAUNCHER;
            } else {
                LOG_ERROR("--give expects pistol, uzi, grenades, strong-grenades or rocket");
                return false;
            }
        } else if (strcmp(a, "--walk-to") == 0 && has_val) {
            o->has_walk_to = true;
            o->walk_to = strtof(argv[++i], NULL);
        } else if (strcmp(a, "--trace") == 0 && has_val) {
            o->trace_every = (u32)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(a, "--expect-video-driver") == 0 && has_val) {
            o->expect_driver = argv[++i];
        } else {
            print_usage(argv[0]);
            return false;
        }
    }
    if (o->shot_every > 0) {
        const char *pct = o->shot ? strchr(o->shot, '%') : NULL;
        if (!pct || strncmp(pct, "%u", 2) != 0 || strchr(pct + 1, '%') != NULL) {
            LOG_ERROR("--shot-every needs --screenshot with exactly one %%u in the path");
            return false;
        }
    }
    return true;
}

static void print_trace(const Sim *sim) {
    const Companion *c = game_player_const(&sim->gs);
    printf("t=%" PRIu64 " pos=(%.2f,%.2f) vel=(%.1f,%.1f) ground=%d ladder=%d\n", sim->gs.tick,
           (f64)c->body.pos.x, (f64)c->body.pos.y, (f64)c->body.vel.x, (f64)c->body.vel.y,
           (int)c->body.on_ground, (int)c->on_ladder);
}

static void print_summary(const Sim *sim) {
    const Companion *c = game_player_const(&sim->gs);
    printf("ticks=%" PRIu64 " pos=(%.2f, %.2f) respawns=%u products=%u soldiers=%u/%u order=%s hp=%d weapon=%s ammo=%d shots=%u hits=%u "
           "kills=%u deaths=%u hash=%016" PRIx64 "\n",
           sim->gs.tick, (f64)c->body.pos.x, (f64)c->body.pos.y, sim->gs.respawns, sim->gs.products_built,
           team_soldier_count(&sim->gs, 0), team_soldier_count(&sim->gs, 1), order_name(sim->gs.teams[0].order), c->hp, weapon_name(c->weapon), c->ammo, sim->gs.combat_stats.shots, sim->gs.combat_stats.hits,
           sim->gs.combat_stats.kills, sim->gs.combat_stats.deaths, game_hash(&sim->gs));
}

static bool write_shot(Render *render, const Sim *sim, u32 flags, const char *path) {
    draw_game(render, &sim->gs, &sim->cam, &sim->menu, &sim->fx, sim->mouse_active ? &sim->mouse_logical : NULL, 1.0f, flags);
    return render_screenshot(render, path);
}

static int run_headless(const Options *o, Sim *sim) {
    Render *render = render_create(true, "Crate Squad");
    if (!render) {
        return 1;
    }
    const u32 flags = o->debug_overlay ? DRAW_DEBUG : 0u;
    u32 frames = o->frames;
    if (sim->use_script && !o->frames_set) {
        frames = script_total_ticks(&sim->script);
    }

    Walker walker;
    walker_init(&walker, o->walk_to);
    InputFrame walk_input = {0};

    int rc = 0;
    for (u32 i = 0; i < frames && rc == 0; i++) {
        if (o->has_walk_to) {
            walker_think(&walker, &sim->level, game_player(&sim->gs), &walk_input);
        }
        sim_tick(sim, o->has_walk_to ? &walk_input : NULL);
        if (o->trace_every > 0 && sim->gs.tick % o->trace_every == 0) {
            print_trace(sim);
        }
        if (o->shot_every > 0 && sim->gs.tick % o->shot_every == 0) {
            char path[512];
            snprintf(path, sizeof(path), o->shot, (unsigned)sim->gs.tick);
            if (!write_shot(render, sim, flags, path)) {
                rc = 1;
            }
        }
    }
    if (rc == 0 && o->shot && o->shot_every == 0) {
        if (o->has_shot_cam) {
            camera_set(&sim->cam, o->shot_cam, &sim->level);
        }
        if (!write_shot(render, sim, flags, o->shot)) {
            rc = 1;
        }
    }
    print_summary(sim);
    render_destroy(render);
    return rc;
}

// Interactive loop. With --expect-video-driver dummy it runs without any real window.
static int run_windowed(const Options *o, Sim *sim) {
    Render *render = render_create(false, "Crate Squad");
    if (!render) {
        return 1;
    }
    u32 flags = o->debug_overlay ? DRAW_DEBUG : 0u;
    bool paused = false, slow = false;
    InputFrame human = {0};
    char title[96];
    snprintf(title, sizeof(title), "Crate Squad");

    f64 prev = platform_time_s();
    f64 acc = 0.0;
    f64 fps_clock = 0.0;
    u32 frames = 0, ticks = 0;
    int rc = 0;
    bool running = true;

    while (running) {
        PlatformEvents ev = {0};
        platform_pump_events(&ev);
        if (ev.quit) {
            break;
        }
        if (ev.toggle_debug) {
            flags ^= DRAW_DEBUG;
        }
        if (ev.toggle_pause) {
            paused = !paused;
        }
        if (ev.restart) {
            sim_restart(sim);
        }
        if (ev.toggle_slow) {
            slow = !slow;
        }
        if (ev.click) {
            sim_click(sim, render_window_to_logical(render, ev.click_x, ev.click_y));
        }
        if (ev.mouse_moved || sim->mouse_active) { // once the mouse was used it keeps aiming and firing
            f32 mx, my;
            bool left;
            platform_read_mouse(&mx, &my, &left);
            sim_set_mouse(sim, render_window_to_logical(render, mx, my), left);
        }

        const f64 now = platform_time_s();
        f64 frame_dt = now - prev;
        prev = now;
        if (frame_dt > 0.25) {
            frame_dt = 0.25;
        }
        fps_clock += frame_dt;
        if (!paused) {
            acc += frame_dt * (slow ? 0.25 : 1.0);
        }

        int to_run = 0;
        if (!paused) {
            while (acc >= (f64)TICK_DT) {
                to_run++;
                acc -= (f64)TICK_DT;
            }
        } else if (ev.step) {
            to_run = 1;
        }
        for (int i = 0; i < to_run; i++) {
            platform_read_input(&human);
            sim_tick(sim, &human);
            ticks++;
        }

        const bool last = o->max_ticks > 0 && sim->gs.tick >= o->max_ticks;
        const f32 alpha = paused ? 1.0f : (f32)(acc / (f64)TICK_DT);
        draw_game(render, &sim->gs, &sim->cam, &sim->menu, &sim->fx, sim->mouse_active ? &sim->mouse_logical : NULL, alpha, flags);
        if (last && o->shot && !render_screenshot(render, o->shot)) {
            rc = 1;
        }
        render_end(render);
        frames++;

        if (fps_clock >= 1.0) {
            snprintf(title, sizeof(title), "Crate Squad - %u FPS / %u TPS",
                     (unsigned)((f64)frames / fps_clock + 0.5), (unsigned)((f64)ticks / fps_clock + 0.5));
            render_set_title(render, title);
            fps_clock = 0.0;
            frames = 0;
            ticks = 0;
        }
        if (last) {
            running = false;
        } else {
            platform_sleep_ms(1); // do not spin when vsync is unavailable
        }
    }
    printf("title=%s\n", render_title(render));
    print_summary(sim);
    render_destroy(render);
    return rc;
}

int main(int argc, char **argv) {
    Options o;
    if (!parse_args(argc, argv, &o)) {
        return 2;
    }
    if (!platform_init(o.headless)) {
        return 1;
    }
    if (o.expect_driver && !o.headless && strcmp(platform_video_driver(), o.expect_driver) != 0) {
        LOG_ERROR("expected video driver '%s' but SDL chose '%s'; refusing to open a window",
                  o.expect_driver, platform_video_driver());
        platform_shutdown();
        return 3;
    }

    char level_path[1024];
    if (o.level) {
        snprintf(level_path, sizeof(level_path), "%s", o.level);
    } else {
        snprintf(level_path, sizeof(level_path), "%s/levels/test_arena.lvl", o.assets);
    }
    SimConfig cfg = {.level_path = level_path,
                     .level_text = NULL,
                     .seed = o.seed,
                     .use_bot = o.bot,
                     .script = o.script,
                     .has_start = o.has_start,
                     .start_feet = o.start_feet};
    Sim sim;
    char err[256] = "";
    if (!sim_init(&sim, &cfg, err, sizeof(err))) {
        LOG_ERROR("%s", err);
        platform_shutdown();
        return 1;
    }

    for (u32 i = 0; i < o.crate_count; i++) {
        crate_spawn(&sim.gs, o.crate_kind[i], o.crate_pos[i], CRATE_FALLING, -1);
    }

    if (o.has_fill_pad && !pad_debug_fill(&sim.gs, 0, o.fill_top, o.fill_bottom)) {
        LOG_ERROR("--fill-pad: the level has no pad for team 1");
        sim_free(&sim);
        platform_shutdown();
        return 1;
    }
    for (u32 i = 0; i < o.soldier_count; i++) {
        const Vec2 feet = vec2(o.soldier_x[i], sim.level.spawns[0].y);
        if (soldier_spawn(&sim.gs, o.soldier_team[i], o.soldier_variant[i], feet) < 0) {
            LOG_ERROR("--soldier: no room for a soldier at x=%.0f", (f64)o.soldier_x[i]);
            sim_free(&sim);
            platform_shutdown();
            return 1;
        }
    }
    if (o.has_order) {
        sim.gs.teams[PLAYER_TEAM].order = o.order;
    }
    if (o.has_give) {
        combat_give_weapon(&sim.gs, o.give);
    }
    pad_menu_refresh(&sim.menu, &sim.gs, 0);

    int rc = o.headless ? run_headless(&o, &sim) : run_windowed(&o, &sim);
    sim_free(&sim);
    platform_shutdown();
    return rc;
}
