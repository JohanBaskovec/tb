// Everything needed to run the game without a renderer: level, state, camera and input source.
// Shared by the headless and windowed loops, and by integration tests.
#pragma once

#include "app/debug_menu.h"
#include "app/fx.h"
#include "app/goal_menu.h"
#include "app/pad_menu.h"
#include "app/sfx.h"
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
    u32 ai_teams;       // bit per team index run by the AI (a commander; for team 0 this means no human at all)
    bool ai_opponents;  // also the AI for every other team that has a spawn and a pad
    u32 start_squad;    // soldiers each team in the match starts with (0 or 1: just one)
} SimConfig;

typedef struct Sim {
    Arena arena; // owns the level tiles
    Level level;
    GameState gs;
    PadMenu menu; // construction menu of the pad of the controlled team (team 0)
    DebugMenu debug; // the F2 spawn menu
    GoalMenu goal_menu; // the middle-click build-goal ring
    FxList fx; // visual effects of the recent events
    SfxState sfx;    // which sounds the recent events make...
    SfxCueList cues; // ... waiting for the window loop to play them (it empties the list)
    Camera cam;
    Bot bot;
    Script script;
    u64 seed;
    bool has_start;
    Vec2 start_feet;
    bool use_bot;
    bool use_script;
    u32 ai_teams; // teams run by the AI, once ai_opponents is resolved
    u32 start_squad;
    InputFrame input; // last frame fed to the game
    // Mouse (window loop only): once it has moved, the soldier aims at it and the left button fires.
    bool mouse_active;
    Vec2 mouse_logical; // logical (640x360) pixels
    bool mouse_left, mouse_right;
    bool mouse_fire_blocked; // a click built a pad entry: the button does not fire until released
    bool mouse_click_pending; // a click outside the menu: fires on the next tick even if the button is up again
} Sim;

bool sim_init(Sim *sim, const SimConfig *cfg, char *err, usize err_len);
void sim_free(Sim *sim);
// Starts the match over (same level, same seed): soldiers, crates and counters are reset. Input sources keep going.
void sim_restart(Sim *sim);
// One fixed tick. Input comes from the script, else the bot, else `human` (may be NULL).
void sim_tick(Sim *sim, const InputFrame *human);
// True while a script is configured and not yet finished.
bool sim_script_running(const Sim *sim);
// Tells the sim where the mouse is (logical pixels) and which buttons are held. From the first call on, human input
// is completed with the aim, ACT_FIRE while the left button is held (unless the pointer is over a pad menu entry, or
// the button went down on one) and ACT_DESTROY while the right one is.
void sim_set_mouse(Sim *sim, Vec2 logical, bool left_down, bool right_down);
// Mouse click at a position in logical (640x360) pixels: spawns the entry of the debug menu under it, else builds
// the pad menu entry under it, if any. Returns true if something was built or spawned. Applied between ticks, like the
// AI's builds.
bool sim_click(Sim *sim, Vec2 logical);
// Middle click: opens the build-goal ring around the pointer, or closes it. Only for a human team that has a pad. While the
// ring is open the mouse belongs to it (no shot, no breaking crates); a click on an entry sets the goal, on the middle disc
// clears it, elsewhere (or a right click) just closes the ring.
void sim_toggle_goal_menu(Sim *sim, Vec2 logical);
// The F2 spawn menu: show / hide it, and put an item next to the soldier being followed (or, with nobody to follow,
// in the middle of the screen). Returns false if there was no room.
void sim_toggle_debug_menu(Sim *sim);
bool sim_debug_spawn(Sim *sim, DebugItem item);
// Logical screen pixels -> world pixels, for the current camera (same rounding as the renderer).
Vec2 sim_logical_to_world(const Sim *sim, Vec2 logical);
