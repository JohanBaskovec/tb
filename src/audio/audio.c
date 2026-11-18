#include "audio/audio.h"

#include "core/log.h"

#include <SDL3/SDL.h>
#include <stdlib.h>

#define VOICES 12
#define MASTER_VOLUME 0.6f
#define MUSIC_VOLUME 0.45f   // under the sound effects
#define MUSIC_CHUNK 1024     // frames rendered at a time on the audio thread

typedef struct Sound {
    i16 *samples; // mono
    u32 count;
} Sound;

struct Audio {
    SDL_AudioDeviceID device;
    SDL_AudioStream *voices[VOICES];
    u32 next_voice;
    Sound sounds[SFX_COUNT];
    i16 *bank;    // all the sounds, one after the other
    i16 *scratch; // stereo copy of the sound being started (2 * SFX_MAX_SAMPLES)
    SDL_AudioStream *music_stream; // NULL without music
    MusicPlayer *music;            // touched by the audio thread: only under the stream's lock
    bool music_on;                 // the player's choice (M): off until asked for
    i16 music_buf[MUSIC_CHUNK * 2];
};

// SDL asks for more music (audio thread, the stream is locked).
static void SDLCALL feed_music(void *userdata, SDL_AudioStream *stream, int additional, int total) {
    (void)total;
    Audio *a = (Audio *)userdata;
    const int frame_bytes = (int)(2 * sizeof(i16));
    while (additional > 0) {
        const int frames = MIN(MUSIC_CHUNK, (additional + frame_bytes - 1) / frame_bytes);
        music_render(a->music, a->music_buf, (u32)frames);
        SDL_PutAudioStreamData(stream, a->music_buf, frames * frame_bytes);
        additional -= frames * frame_bytes;
    }
}

static bool open_music(Audio *a, const SDL_AudioSpec *spec) {
    a->music = (MusicPlayer *)calloc(1, sizeof(*a->music));
    if (!a->music) {
        return false;
    }
    if (!music_init(a->music)) {
        LOG_WARN("no music: %s", a->music->error);
        free(a->music);
        a->music = NULL;
        return true; // the sound effects still play
    }
    a->music_stream = SDL_CreateAudioStream(spec, spec);
    if (!a->music_stream || !SDL_SetAudioStreamGetCallback(a->music_stream, feed_music, a) ||
        !SDL_SetAudioStreamGain(a->music_stream, MUSIC_VOLUME) || !SDL_BindAudioStream(a->device, a->music_stream)) {
        LOG_WARN("no music: %s", SDL_GetError());
        return false;
    }
    return true;
}

const char *audio_driver(void) {
    const char *name = SDL_GetCurrentAudioDriver();
    return name ? name : "";
}

Audio *audio_create(bool music) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        LOG_WARN("no audio: %s", SDL_GetError());
        return NULL;
    }
    Audio *a = (Audio *)calloc(1, sizeof(*a));
    if (!a) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return NULL;
    }
    const SDL_AudioSpec spec = {SDL_AUDIO_S16, 2, SFX_RATE};
    a->device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    a->bank = (i16 *)calloc((usize)SFX_COUNT * SFX_MAX_SAMPLES, sizeof(i16));
    a->scratch = (i16 *)calloc((usize)SFX_MAX_SAMPLES * 2, sizeof(i16));
    if (!a->device || !a->bank || !a->scratch) {
        LOG_WARN("no audio: %s", SDL_GetError());
        audio_destroy(a);
        return NULL;
    }
    for (int v = 0; v < VOICES; v++) {
        a->voices[v] = SDL_CreateAudioStream(&spec, &spec);
        if (!a->voices[v] || !SDL_BindAudioStream(a->device, a->voices[v])) {
            LOG_WARN("audio voice %d: %s", v, SDL_GetError());
            audio_destroy(a);
            return NULL;
        }
    }
    if (music && !open_music(a, &spec)) {
        audio_destroy(a);
        return NULL;
    }
    for (int id = 0; id < SFX_COUNT; id++) {
        Sound *s = &a->sounds[id];
        s->samples = a->bank + (usize)id * SFX_MAX_SAMPLES;
        s->count = sfx_synthesize((SfxId)id, s->samples, SFX_MAX_SAMPLES);
    }
    LOG_INFO("audio: %s, %d voices, music %s", audio_driver(), VOICES, a->music_stream ? "on" : "off");
    return a;
}

void audio_destroy(Audio *a) {
    if (!a) {
        return;
    }
    for (int v = 0; v < VOICES; v++) {
        if (a->voices[v]) {
            SDL_DestroyAudioStream(a->voices[v]);
        }
    }
    if (a->music_stream) {
        SDL_DestroyAudioStream(a->music_stream); // stops the callback before the player goes
    }
    free(a->music);
    if (a->device) {
        SDL_CloseAudioDevice(a->device);
    }
    free(a->bank);
    free(a->scratch);
    free(a);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

// A free voice, else the one that has the least left to play.
static SDL_AudioStream *pick_voice(Audio *a) {
    SDL_AudioStream *best = NULL;
    int best_left = 0;
    for (int k = 0; k < VOICES; k++) {
        SDL_AudioStream *v = a->voices[(a->next_voice + (u32)k) % VOICES];
        const int left = SDL_GetAudioStreamQueued(v);
        if (left <= 0) {
            best = v;
            break;
        }
        if (!best || left < best_left) {
            best = v;
            best_left = left;
        }
    }
    a->next_voice = (a->next_voice + 1) % VOICES;
    return best;
}

void audio_play(Audio *a, const SfxCue *cue) {
    if (!a || cue->id < 0 || cue->id >= SFX_COUNT) {
        return;
    }
    const Sound *s = &a->sounds[cue->id];
    const f32 vol = cue->volume * MASTER_VOLUME;
    const f32 left = vol * (cue->pan > 0.0f ? 1.0f - cue->pan : 1.0f);
    const f32 right = vol * (cue->pan < 0.0f ? 1.0f + cue->pan : 1.0f);
    for (u32 i = 0; i < s->count; i++) {
        a->scratch[2 * i] = (i16)((f32)s->samples[i] * left);
        a->scratch[2 * i + 1] = (i16)((f32)s->samples[i] * right);
    }
    SDL_AudioStream *voice = pick_voice(a);
    SDL_ClearAudioStream(voice);
    SDL_PutAudioStreamData(voice, a->scratch, (int)(s->count * 2 * sizeof(i16)));
}

void audio_music(Audio *a, MusicTrack track, f32 volume) {
    if (!a || !a->music_stream) {
        return;
    }
    SDL_LockAudioStream(a->music_stream);
    music_play(a->music, a->music_on ? track : MUSIC_NONE, volume); // off: the song fades out, the stream stays open
    SDL_UnlockAudioStream(a->music_stream);
}

void audio_play_all(Audio *a, SfxCueList *cues) {
    for (u32 i = 0; i < cues->count; i++) {
        audio_play(a, &cues->items[i]);
    }
    cues->count = 0;
}

bool audio_set_music_enabled(Audio *a, bool on) {
    if (!a || !a->music_stream) {
        return false;
    }
    a->music_on = on;
    return true;
}

bool audio_music_enabled(const Audio *a) {
    return a && a->music_stream && a->music_on;
}
