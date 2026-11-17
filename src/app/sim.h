// Everything needed to run the game without a renderer: level, state, camera and input source.
// Shared by the headless and windowed loops, and by integration tests.
#pragma once

#include "app/pad_menu.h"
#include "core/arena.h"
#include "game/bot.h"
#include "game/game.h"
#include "game/script.h"
#include "render/camera.h"

typedef struct SimConfig {
    const char *level_path; // either a file...
    const char *level_text; // ...or the level text itself (tests)
    u64 seed;
    bool use_bot;
    const char *script; // optional input script (takes precedence over the bot)
    bool has_start;     // override the team-1 spawn
    Vec2 start_feet;    // bottom-center of the body, in pixels
} SimConfig;

typedef struct Sim {
    Arena arena; // owns the level tiles
    Level level;
    GameState gs;
    PadMenu menu; // construction menu of the pad of the controlled team (team 0)
    Camera cam;
    Bot bot;
    Script script;
    bool use_bot;
    bool use_script;
    InputFrame input; // last frame fed to the game
} Sim;

bool sim_init(Sim *sim, const SimConfig *cfg, char *err, usize err_len);
void sim_free(Sim *sim);
// One fixed tick. Input comes from the script, else the bot, else `human` (may be NULL).
void sim_tick(Sim *sim, const InputFrame *human);
// True while a script is configured and not yet finished.
bool sim_script_running(const Sim *sim);
// Mouse click at a position in logical (640x360) pixels: builds the pad menu entry under it, if any.
// Returns true if something was built. Applied between ticks, like the AI's builds.
bool sim_click(Sim *sim, Vec2 logical);
// Logical screen pixels -> world pixels, for the current camera (same rounding as the renderer).
Vec2 sim_logical_to_world(const Sim *sim, Vec2 logical);
