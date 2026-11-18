// Sound output over SDL: every placeholder sound (app/sfx.h) is synthesized once at start-up, then played on
// a few voices that SDL mixes. The music (app/music.h) has its own stream, filled on SDL's audio thread by
// music_render. Never used in headless mode.
#pragma once

#include "app/music.h"
#include "app/sfx.h"

typedef struct Audio Audio;

// Opens the default playback device. Returns NULL (and the game stays silent) if there is none.
// Without `music`, only the sound effects play. With it, the music is still off until audio_set_music_enabled.
Audio *audio_create(bool music);
void audio_destroy(Audio *a);
// Name of the SDL audio driver in use ("pulseaudio", "dummy"...), or "" when audio is not initialized.
const char *audio_driver(void);
void audio_play(Audio *a, const SfxCue *cue);
// Plays every cue of the list and empties it.
void audio_play_all(Audio *a, SfxCueList *cues);
// The music to hear (see music_play). Cheap: call it every frame. Silent while the music is switched off.
void audio_music(Audio *a, MusicTrack track, f32 volume);
// Switches the music on or off (off at start). Returns false, and does nothing, without a music stream
// (--no-music, or no device).
bool audio_set_music_enabled(Audio *a, bool on);
bool audio_music_enabled(const Audio *a);
