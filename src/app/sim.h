// Everything needed to run the game without a renderer: level, state, camera and input source.
// Shared by the headless and windowed loops, and by integration tests.
#pragma once

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
