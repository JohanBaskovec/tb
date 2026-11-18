// Sound output over SDL: every placeholder sound (app/sfx.h) is synthesized once at start-up, then played on
// a few voices that SDL mixes. Never used in headless mode.
#pragma once

#include "app/sfx.h"

typedef struct Audio Audio;

// Opens the default playback device. Returns NULL (and the game stays silent) if there is none.
Audio *audio_create(void);
void audio_destroy(Audio *a);
// Name of the SDL audio driver in use ("pulseaudio", "dummy"...), or "" when audio is not initialized.
const char *audio_driver(void);
void audio_play(Audio *a, const SfxCue *cue);
// Plays every cue of the list and empties it.
void audio_play_all(Audio *a, SfxCueList *cues);
