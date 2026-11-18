// Placeholder sound effects: synthesized from a few parameters (no sound file), and picked from the game's
// events. No SDL: playing them is audio/audio.c's job.
//
// Every tick, sfx_consume turns the events of that tick into cues (a sound, a volume, a pan) placed relative
// to a listener (the center of the camera): far events are quieter, then silent; the same sound is not repeated
// faster than its minimum gap (a burst of hits makes one sound, not twenty).
#pragma once

#include "game/events.h"

typedef enum SfxId {
    SFX_SHOT_PISTOL,
    SFX_SHOT_UZI,
    SFX_THROW,      // grenade or crate thrown
    SFX_ROCKET,
    SFX_EXPLOSION,
    SFX_PUNCH,
    SFX_HIT,
    SFX_CRATE_PICK,
    SFX_CRATE_PLACE,
    SFX_CRATE_DROP, // a crate leaves the sky
    SFX_BUILD,
    SFX_SPAWN,
    SFX_DEATH,
    SFX_SPLASH,
    SFX_WEAPON_PICKUP,
    SFX_EMPTY,
    SFX_ORDER,
    SFX_BASE_DESTROYED,
    SFX_VICTORY,
    SFX_DEFEAT,
    SFX_MENU_MOVE,
    SFX_MENU_OK,
    SFX_MENU_BACK,
    SFX_EXPLOSION_BIG, // rockets, strong grenades
    SFX_RICOCHET,      // a bullet hits a wall
    SFX_JUMP,
    SFX_LAND,          // after a real fall
    SFX_CRATE_LAND,    // a crate settles
    SFX_DUMMY_BREAK,
    SFX_WEAPON_DROP,
    SFX_SWITCH,        // the player takes control of another soldier
    SFX_ELIMINATED,    // a team is out (while the match goes on)
    SFX_ALARM,         // the player's base is under attack
    SFX_PAD_OPEN,      // the pad menu appears
    SFX_COUNT
} SfxId;

#define SFX_RATE 44100
#define SFX_MAX_SAMPLES (SFX_RATE * 3 / 2) // the longest sound: 1.5 s

typedef struct SfxCue {
    SfxId id;
    f32 volume; // 0..1
    f32 pan;    // -1 left .. +1 right
} SfxCue;

#define SFX_MAX_CUES 32

// Cues waiting to be played (the window loop drains them after each frame).
typedef struct SfxCueList {
    SfxCue items[SFX_MAX_CUES];
    u32 count;
} SfxCueList;

typedef struct SfxState {
    u32 cooldown[SFX_COUNT]; // ticks before the sound may play again
    i32 human_team;          // the team whose victory is a victory (-1: nobody, AI against AI)
    i32 focus_soldier;       // the soldier the camera follows: only its jumps and landings are heard (-1: none)
} SfxState;

// Mono 16-bit samples of a sound at SFX_RATE; returns their number (at most `cap`). Deterministic.
u32 sfx_synthesize(SfxId id, i16 *out, u32 cap);
const char *sfx_name(SfxId id);
// Adds a cue (dropped when the list is full; a quieter duplicate of this frame is merged).
void sfx_push(SfxCueList *list, SfxId id, f32 volume, f32 pan);
// The events of one tick -> cues, heard from `listener` (world pixels). Call once per tick.
void sfx_consume(SfxState *state, const EventQueue *events, Vec2 listener, SfxCueList *out);
// A sound that does not come from an event (the pad menu...): its usual volume, centered, gap respected.
void sfx_play_ui(SfxState *state, SfxId id, SfxCueList *out);
