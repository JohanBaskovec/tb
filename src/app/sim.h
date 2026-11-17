// Everything needed to run the game without a renderer: level, state, camera and input source.
// Shared by the headless and windowed loops, and by integration tests.
#pragma once

#include "app/fx.h"
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
    FxList fx; // visual effects of the recent events
    Camera cam;
    Bot bot;
    Script script;
    bool use_bot;
    bool use_script;
    InputFrame input; // last frame fed to the game
    // Mouse (window loop only): once it has moved, the soldier aims at it and the left button fires.
    bool mouse_active;
    Vec2 mouse_logical; // logical (640x360) pixels
    bool mouse_left;
    bool mouse_fire_blocked; // a click built a pad entry: the button does not fire until released
    bool mouse_click_pending; // a click outside the menu: fires on the next tick even if the button is up again
} Sim;

bool sim_init(Sim *sim, const SimConfig *cfg, char *err, usize err_len);
void sim_free(Sim *sim);
// One fixed tick. Input comes from the script, else the bot, else `human` (may be NULL).
void sim_tick(Sim *sim, const InputFrame *human);
// True while a script is configured and not yet finished.
bool sim_script_running(const Sim *sim);
// Tells the sim where the mouse is (logical pixels) and whether its left button is held. From the first
// call on, human input is completed with the aim and, while the button is held, ACT_FIRE (unless the
// pointer is over a pad menu entry, or the button went down on one).
void sim_set_mouse(Sim *sim, Vec2 logical, bool left_down);
// Mouse click at a position in logical (640x360) pixels: builds the pad menu entry under it, if any.
// Returns true if something was built. Applied between ticks, like the AI's builds.
bool sim_click(Sim *sim, Vec2 logical);
// Logical screen pixels -> world pixels, for the current camera (same rounding as the renderer).
Vec2 sim_logical_to_world(const Sim *sim, Vec2 logical);
