// The screens around a match: title, match setup, the match itself, pause and results. No SDL: menus are
// driven by InputFrames (and mouse positions in logical pixels), so the whole flow is testable headless.
//
// Keys in the menus: up / down choose an entry, left / right change a setting, confirm (Enter) or jump
// (Space) validates, pause (Escape) goes back. In a match, pause (Escape) opens the pause menu.
#pragma once

#include "app/music.h"
#include "app/sim.h"

typedef enum Screen {
    SCREEN_TITLE,
    SCREEN_SETUP,
    SCREEN_PLAY,
    SCREEN_PAUSE,
    SCREEN_RESULTS,
    SCREEN_QUIT, // the player chose to quit: the window loop ends
} Screen;

// Levels offered by the setup screen (files of <assets>/levels).
typedef struct FrontLevel {
    const char *file;
    const char *title;
    MusicTrack music; // played during its matches
} FrontLevel;

#define FRONT_LEVEL_COUNT 3
extern const FrontLevel FRONT_LEVELS[FRONT_LEVEL_COUNT];

// Entries of each menu, top to bottom.
enum { TITLE_PLAY, TITLE_QUIT, TITLE_COUNT };
enum { SETUP_LEVEL, SETUP_MODE, SETUP_SQUAD, SETUP_START, SETUP_BACK, SETUP_COUNT };
enum { PAUSE_RESUME, PAUSE_RESTART, PAUSE_QUIT, PAUSE_COUNT };
enum { RESULTS_AGAIN, RESULTS_MENU, RESULTS_COUNT };

typedef struct FrontConfig {
    const char *assets_dir;
    u64 seed; // of the first match; every new match takes the next one
} FrontConfig;

typedef struct Front {
    Screen screen;
    i32 selected;     // entry of the current menu
    i32 level_index;  // into FRONT_LEVELS
    bool spectate;    // AI against AI: nobody to control
    u32 squad;        // soldiers each team starts with (1..TUNING_SKIRMISH_MAX_SQUAD)
    u64 seed;         // of the match being played (or the next one)
    u32 screen_ticks; // since the current screen opened (animations)
    u32 over_ticks;   // since the match ended
    bool sim_ready;
    Sim sim;
    Arena preview_arena; // level shown by the setup screen
    Level preview;
    bool preview_ok;
    char assets[512];
    char error[192]; // why the last match could not start ("" if it did)
    SfxCueList cues; // menu sounds, waiting for the window loop to play them
} Front;

bool front_init(Front *f, const FrontConfig *cfg);
void front_free(Front *f);
// One fixed tick: the menus read `in` (its edges), the match runs while playing.
void front_tick(Front *f, const InputFrame *in);
// Number of entries of the current menu (0 while playing) and the rectangle of each, in logical pixels:
// the renderer draws them there and a click there selects them.
i32 front_menu_count(const Front *f);
Rect front_menu_rect(const Front *f, i32 index);
// The mouse is over `logical`: the entry under it becomes the selected one.
void front_point(Front *f, Vec2 logical);
// A click at `logical`: on a menu entry, selects and validates it (on a setting, changes it); while playing,
// it goes to the match (pad menu, shooting). Returns true if a menu entry took it.
bool front_click(Front *f, Vec2 logical);
// What should be heard now: the title music in the menus, the level's music during a match (quieter in the
// pause menu), silence once the match is over (the victory or defeat jingle plays), calm music on the results.
MusicTrack front_music(const Front *f, f32 *volume);
// The music of a level file (matched by name against FRONT_LEVELS; the cove's music for any other level).
MusicTrack front_level_music(const char *path);
// The music of a match run without the front (`--play`): silence once it is over.
MusicTrack front_match_music(const Sim *sim, const char *level_path);
// The match in progress, or NULL on the title and setup screens.
Sim *front_sim(Front *f);
const Sim *front_sim_const(const Front *f);
