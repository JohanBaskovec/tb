// Placeholder music: a tiny chiptune sequencer, no sound file and no SDL. Every song is written below in
// music.c as text (chords, a lead melody, a drum plan) and played by four synthesized channels: a pulse lead,
// a pulse accompaniment built from the chords (arpeggio or off-beat chords), a triangle bass and noise drums.
//
// music_render fills stereo 16-bit frames at SFX_RATE; it is deterministic and allocation-free, so the audio
// thread can call it, and tests or `--export-audio` can render a song to a buffer.
//
// music_play asks for a song: the current one fades out, then the new one starts from its beginning (an intro,
// then a loop forever). Asking again for the playing song only changes its volume (smoothly).
#pragma once

#include "app/sfx.h"
#include "core/rng.h"

typedef enum MusicTrack {
    MUSIC_NONE,
    MUSIC_TITLE,   // title and setup screens
    MUSIC_COVE,    // La Crique: sunny, bouncy
    MUSIC_ARENA,   // L'Arène: minor, driving
    MUSIC_RESULTS, // results screen: calm
    MUSIC_COUNT
} MusicTrack;

#define MUSIC_PAUSE_VOLUME 0.35f // the match music while the pause menu is up
#define MUSIC_MAX_BARS 24
#define MUSIC_STEPS_PER_BAR 16 // sixteenth notes
#define MUSIC_MAX_STEPS (MUSIC_MAX_BARS * MUSIC_STEPS_PER_BAR)
#define MUSIC_MAX_CHORD_TONES 4

// A song turned into one entry per sixteenth note.
typedef struct MusicChord {
    u8 root;  // pitch class 0..11 (C = 0)
    u8 count; // tones
    u8 tones[MUSIC_MAX_CHORD_TONES]; // semitones above the root
} MusicChord;

typedef struct MusicSong {
    u32 bpm;
    u32 steps;      // whole song
    u32 loop_step;  // where the loop starts again (after the intro)
    u8 bass_style, accomp_style;
    u8 lead[MUSIC_MAX_STEPS];  // MIDI note, MUSIC_HOLD or MUSIC_REST
    u8 drums[MUSIC_MAX_STEPS]; // MUSIC_DRUM_* bits
    MusicChord chords[MUSIC_MAX_STEPS];
} MusicSong;

#define MUSIC_HOLD 0xFF
#define MUSIC_REST 0x00

#define MUSIC_DRUM_KICK 1u
#define MUSIC_DRUM_SNARE 2u
#define MUSIC_DRUM_HAT 4u
#define MUSIC_DRUM_OPEN_HAT 8u

typedef struct MusicVoice {
    f32 freq, phase;
    f32 level, peak, sustain;
    f32 decay, release; // per-sample multipliers
    u32 attack;         // samples left in the attack
    bool gate;
    f32 duty;           // pulse width (0.5 = square), ignored by the triangle
    f32 held;           // seconds since the note started (vibrato)
} MusicVoice;

typedef struct MusicDrums {
    f32 kick_t, kick_phase; // seconds since the hit (negative: silent)
    f32 snare_t, hat_t, hat_len;
    f32 noise_lp, noise_hp, hat_lp;
    Rng rng;
} MusicDrums;

typedef struct MusicPlayer {
    MusicSong songs[MUSIC_COUNT];
    MusicTrack current, wanted;
    f32 volume, wanted_volume; // of the current song
    f32 gain;                  // fade in / out
    u64 pos;                   // samples since the song (re)started at step 0
    u32 step;                  // step being played
    u32 bass_off;              // samples left before the bass note is released
    MusicVoice lead, bass, accomp[3];
    MusicDrums drums;
    f32 lead_lp, accomp_lp;
    char error[96];            // why music_init failed (a song that does not parse)
} MusicPlayer;

// Parses every song. Returns false (with p->error) if one is malformed: then nothing plays.
bool music_init(MusicPlayer *p);
const char *music_name(MusicTrack track);
// Switches to `track` (fading the current one out) or, if it is already playing, moves to `volume` (0..1).
void music_play(MusicPlayer *p, MusicTrack track, f32 volume);
// Starts `track` from its beginning right away, without a fade (tests, export).
void music_start(MusicPlayer *p, MusicTrack track, f32 volume);
// Writes `frames` interleaved stereo frames (left, right).
void music_render(MusicPlayer *p, i16 *stereo, u32 frames);
// Length of the intro and of the loop of a song, in samples (0 for MUSIC_NONE).
u32 music_intro_samples(const MusicPlayer *p, MusicTrack track);
u32 music_loop_samples(const MusicPlayer *p, MusicTrack track);
