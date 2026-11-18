#include "app/music.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define RATE ((f32)SFX_RATE)
#define TAU 6.28318531f

enum { BASS_WALK, BASS_BOUNCE, BASS_DRIVE, BASS_LONG };
enum { ACC_ARP, ACC_CHUNK, ACC_ROLL };

// ---- The songs (original compositions) ---------------------------------------------------------
//
// chords: one token per bar ("Am", "G7", "Bb", "F#m", "CM7"...), "X,Y" splits the bar in two halves.
// lead:   one token per sixteenth note: a note ("C5", "F#4", "Bb4"), "-" holds it, "." is silence;
//         every bar ends with "|" (the parser checks it has 16 steps).
// drums:  one letter per bar, picking one of the patterns (A, B, C, D) or "." for none. Pattern letters per
//         sixteenth: K kick, S snare, H hat, O open hat, X kick + hat, "." nothing.

typedef struct SongDef {
    const char *name;
    u32 bpm;
    u32 loop_bar;
    u8 bass, accomp;
    const char *chords;
    const char *lead;
    const char *drum_plan;
    const char *drums[4];
} SongDef;

static const SongDef SONGS[MUSIC_COUNT] = {
    [MUSIC_NONE] = {"none", 120, 0, BASS_WALK, ACC_ARP, "", "", "", {NULL}},
    [MUSIC_TITLE] =
        {
            "title",
            112,
            2,
            BASS_WALK,
            ACC_ARP,
            "F C "
            "F Dm Bb C F Am Bb,C F",
            ". - - - - - - - - - - - - - - - | . - - - - - - - - - - - G4 - A4 - |"
            "C5 - F5 - A5 - - - G5 - F5 - A5 - - - | D5 - F5 - A5 - - - C6 - A5 - F5 - - - |"
            "Bb4 - D5 - F5 - - - G5 - F5 - D5 - Bb4 - | C5 - - - E5 - G5 - Bb5 - - - A5 - G5 - |"
            "A5 - - - - - C6 - A5 - F5 - - - G5 - | A5 - - - E5 - - - C5 - E5 - A5 - G5 - |"
            "F5 - D5 - Bb4 - D5 - E5 - G5 - C6 - Bb5 - | A5 - - - - - - - F5 - - - . - - - |",
            "CD"
            "AAAAAAAB",
            {"K.H.S.H.K.H.S.HH", "K.H.S.H.K.S.SSSS", "H.H.H.H.H.H.H.H.", "K...K...K...K.SS"},
        },
    [MUSIC_COVE] =
        {
            "cove",
            140,
            0,
            BASS_BOUNCE,
            ACC_CHUNK,
            "G Em C D G Em C,D G "
            "C D Bm Em Am D C,D G",
            "G4 - B4 - D5 - B4 - G5 - - - D5 - B4 - | E5 - - - G5 - E5 - B4 - - - E5 - G5 - |"
            "A5 - G5 - E5 - C5 - E5 - G5 - - - E5 - | F#5 - - - E5 - D5 - A4 - - - D5 - F#5 - |"
            "G5 - D5 - B4 - D5 - G5 - A5 - B5 - - - | B5 - A5 - G5 - E5 - - - G5 - E5 - B4 - |"
            "C5 - E5 - G5 - E5 - D5 - F#5 - A5 - C6 - | B5 - - - A5 - - - G5 - - - . - - - |"
            "E5 - - - E5 - G5 - C6 - - - B5 - G5 - | A5 - - - F#5 - D5 - A5 - B5 - A5 - F#5 - |"
            "F#5 - - - D5 - B4 - D5 - F#5 - B5 - - - | G5 - - - - - E5 - B4 - E5 - G5 - - - |"
            "C6 - B5 - A5 - - - E5 - - - A5 - C6 - | D6 - - - C6 - B5 - A5 - - - F#5 - - - |"
            "G5 - E5 - C5 - E5 - F#5 - A5 - D6 - C6 - | B5 - - - - - - - G5 - - - - - . - |",
            "AAAAAAAB"
            "CCCCCCCB",
            {"K.H.S.HKK.H.S.H.", "K.H.S.HKS.SSS.SS", "K.HOS.H.K.HOS.HK", NULL},
        },
    [MUSIC_ARENA] =
        {
            "arena",
            150,
            0,
            BASS_DRIVE,
            ACC_ARP,
            "Am Am F G Am Am F,G E "
            "Dm Am F E Dm Am F,G E",
            "A4 - - A4 C5 - A4 - E5 - - - D5 - C5 - | B4 - C5 - B4 - G4 - A4 - - - . - E4 - |"
            "A4 - - A4 C5 - F5 - E5 - - - C5 - A4 - | B4 - - - D5 - G5 - F5 - E5 - D5 - B4 - |"
            "E5 - - E5 A5 - E5 - C6 - - - B5 - A5 - | G5 - A5 - G5 - E5 - A5 - - - . - E5 - |"
            "F5 - A5 - C6 - A5 - G5 - B5 - D6 - B5 - | G#5 - - - E5 - - - B4 - - - G#4 - B4 - |"
            "D5 - - - F5 - A5 - D6 - - - C6 - A5 - | C6 - - - B5 - A5 - E5 - - - . - - - |"
            "F5 - - - A5 - C6 - F6 - - - E6 - C6 - | B5 - - - G#5 - E5 - B5 - - - . - G#5 - |"
            "A5 - - - F5 - D5 - F5 - A5 - D6 - - - | C6 - B5 - A5 - - - E5 - A5 - C6 - - - |"
            "A5 - G5 - F5 - E5 - D5 - E5 - F5 - G5 - | G#5 - - - - - - - B5 - - - E6 - - - |",
            "AAAAAAAC"
            "BBBBBBBC",
            {"K.HKS.H.K.HKS.H.", "X.HKS.HHK.HKS.HS", "K.S.S.SSK.SSSSSS", NULL},
        },
    [MUSIC_RESULTS] =
        {
            "results",
            92,
            0,
            BASS_LONG,
            ACC_ROLL,
            "C Am F G C Em F,G C",
            "E5 - - - - - G5 - - - E5 - D5 - C5 - | A4 - - - - - C5 - E5 - - - - - - - |"
            "F5 - - - E5 - D5 - C5 - - - A4 - C5 - | D5 - - - - - - - B4 - - - . - - - |"
            "E5 - - - G5 - - - C6 - - - B5 - G5 - | B5 - - - - - G5 - E5 - - - . - - - |"
            "A5 - - - G5 - F5 - D5 - - - B4 - D5 - | C5 - - - - - - - - - - - . - - - |",
            "AAAAAAAA",
            {"K.....H.S.....H.", NULL, NULL, NULL},
        },
};

// ---- Parsing -----------------------------------------------------------------------------------

static bool fail(MusicPlayer *p, const char *song, const char *what) {
    snprintf(p->error, sizeof(p->error), "music %s: %s", song, what);
    return false;
}

// Pitch class of a note letter with its optional '#' / 'b'; advances `*s`. -1 if not a note.
static i32 pitch_class(const char **s) {
    static const i32 letters[7] = {9, 11, 0, 2, 4, 5, 7}; // A B C D E F G
    const char c = **s;
    if (c < 'A' || c > 'G') {
        return -1;
    }
    i32 pc = letters[c - 'A'];
    (*s)++;
    if (**s == '#') {
        pc++;
        (*s)++;
    } else if (**s == 'b') {
        pc--;
        (*s)++;
    }
    return (pc + 12) % 12;
}

// One chord token (up to `end`): root, then "", "m", "7", "m7" or "M7".
static bool parse_chord(const char *s, const char *end, MusicChord *out) {
    const i32 pc = pitch_class(&s);
    if (pc < 0) {
        return false;
    }
    char q[4] = "";
    const usize n = (usize)(end - s);
    if (n >= sizeof(q)) {
        return false;
    }
    memcpy(q, s, n);
    q[n] = '\0';
    static const struct {
        const char *name;
        u8 count, tones[4];
    } QUALITIES[] = {{"", 3, {0, 4, 7}}, {"m", 3, {0, 3, 7}}, {"7", 4, {0, 4, 7, 10}}, {"m7", 4, {0, 3, 7, 10}}, {"M7", 4, {0, 4, 7, 11}}};
    for (usize k = 0; k < sizeof(QUALITIES) / sizeof(QUALITIES[0]); k++) {
        if (strcmp(q, QUALITIES[k].name) == 0) {
            out->root = (u8)pc;
            out->count = QUALITIES[k].count;
            memcpy(out->tones, QUALITIES[k].tones, sizeof(out->tones));
            return true;
        }
    }
    return false;
}

static const char *skip_spaces(const char *s) {
    while (*s == ' ' || *s == '\n') {
        s++;
    }
    return s;
}

// A token ends at a space; "|" is a token of its own even when glued to its neighbours.
static const char *token_end(const char *s) {
    if (*s == '|') {
        return s + 1;
    }
    while (*s && *s != ' ' && *s != '\n' && *s != '|') {
        s++;
    }
    return s;
}

static bool parse_chords(MusicPlayer *p, const SongDef *d, MusicSong *song, u32 *bars) {
    *bars = 0;
    for (const char *s = skip_spaces(d->chords); *s; s = skip_spaces(s)) {
        const char *end = token_end(s);
        if (*bars >= MUSIC_MAX_BARS) {
            return fail(p, d->name, "too many bars");
        }
        const char *comma = memchr(s, ',', (usize)(end - s));
        MusicChord first = {0}, second = {0};
        if (!parse_chord(s, comma ? comma : end, &first) || (comma && !parse_chord(comma + 1, end, &second))) {
            return fail(p, d->name, "bad chord");
        }
        for (u32 k = 0; k < MUSIC_STEPS_PER_BAR; k++) {
            song->chords[*bars * MUSIC_STEPS_PER_BAR + k] = (comma && k >= MUSIC_STEPS_PER_BAR / 2) ? second : first;
        }
        (*bars)++;
        s = end;
    }
    return *bars > 0 || d->chords[0] == '\0';
}

static bool parse_lead(MusicPlayer *p, const SongDef *d, MusicSong *song) {
    u32 step = 0, bar_start = 0;
    for (const char *s = skip_spaces(d->lead); *s; s = skip_spaces(s)) {
        const char *end = token_end(s);
        if (*s == '|' && end == s + 1) {
            if (step - bar_start != MUSIC_STEPS_PER_BAR) {
                return fail(p, d->name, "a lead bar is not 16 steps long");
            }
            bar_start = step;
        } else {
            if (step >= song->steps) {
                return fail(p, d->name, "the lead is longer than the chords");
            }
            u8 note;
            if (*s == '-' && end == s + 1) {
                note = MUSIC_HOLD;
            } else if (*s == '.' && end == s + 1) {
                note = MUSIC_REST;
            } else {
                const char *t = s;
                const i32 pc = pitch_class(&t);
                if (pc < 0 || t + 1 != end || *t < '1' || *t > '7') {
                    return fail(p, d->name, "bad lead note");
                }
                note = (u8)((*t - '0' + 1) * 12 + pc);
            }
            song->lead[step++] = note;
        }
        s = end;
    }
    if (step != song->steps || bar_start != step) {
        return fail(p, d->name, "the lead and the chords do not have the same bars");
    }
    return true;
}

static bool parse_drums(MusicPlayer *p, const SongDef *d, MusicSong *song, u32 bars) {
    if (strlen(d->drum_plan) != bars) {
        return fail(p, d->name, "the drum plan needs one letter per bar");
    }
    for (u32 b = 0; b < bars; b++) {
        const char c = d->drum_plan[b];
        const char *pattern = NULL;
        if (c >= 'A' && c <= 'D') {
            pattern = d->drums[c - 'A'];
            if (!pattern || strlen(pattern) != MUSIC_STEPS_PER_BAR) {
                return fail(p, d->name, "a drum pattern is missing or not 16 steps long");
            }
        } else if (c != '.') {
            return fail(p, d->name, "bad drum plan letter");
        }
        for (u32 k = 0; k < MUSIC_STEPS_PER_BAR; k++) {
            u8 bits = 0;
            switch (pattern ? pattern[k] : '.') {
            case 'K': bits = MUSIC_DRUM_KICK; break;
            case 'S': bits = MUSIC_DRUM_SNARE; break;
            case 'H': bits = MUSIC_DRUM_HAT; break;
            case 'O': bits = MUSIC_DRUM_OPEN_HAT; break;
            case 'X': bits = MUSIC_DRUM_KICK | MUSIC_DRUM_HAT; break;
            case '.': break;
            default: return fail(p, d->name, "bad drum letter");
            }
            song->drums[b * MUSIC_STEPS_PER_BAR + k] = bits;
        }
    }
    return true;
}

static bool parse_song(MusicPlayer *p, MusicTrack track) {
    const SongDef *d = &SONGS[track];
    MusicSong *song = &p->songs[track];
    memset(song, 0, sizeof(*song));
    song->bpm = d->bpm;
    song->bass_style = d->bass;
    song->accomp_style = d->accomp;
    u32 bars;
    if (!parse_chords(p, d, song, &bars)) {
        return false;
    }
    song->steps = bars * MUSIC_STEPS_PER_BAR;
    if (d->loop_bar >= bars && bars > 0) {
        return fail(p, d->name, "the loop starts after the end");
    }
    song->loop_step = d->loop_bar * MUSIC_STEPS_PER_BAR;
    return parse_lead(p, d, song) && parse_drums(p, d, song, bars);
}

bool music_init(MusicPlayer *p) {
    memset(p, 0, sizeof(*p));
    rng_seed(&p->drums.rng, 0x5EED, 3);
    for (int t = MUSIC_TITLE; t < MUSIC_COUNT; t++) {
        if (!parse_song(p, (MusicTrack)t)) {
            memset(p->songs, 0, sizeof(p->songs)); // nothing plays rather than garbage
            return false;
        }
    }
    return true;
}

const char *music_name(MusicTrack track) {
    return track >= 0 && track < MUSIC_COUNT ? SONGS[track].name : "?";
}

// ---- Timing ------------------------------------------------------------------------------------

// First sample of a step: steps are 60 / (bpm * 4) s long, rounded so that the song never drifts.
static u64 step_start(const MusicSong *s, u32 step) {
    const u64 per_minute = (u64)s->bpm * 4u, num = (u64)step * 60u * (u64)SFX_RATE;
    return (num + per_minute - 1u) / per_minute;
}

static u32 step_at(const MusicSong *s, u64 pos) {
    return (u32)(pos * ((u64)s->bpm * 4u) / (60u * (u64)SFX_RATE));
}

u32 music_intro_samples(const MusicPlayer *p, MusicTrack track) {
    if (track <= MUSIC_NONE || track >= MUSIC_COUNT || p->songs[track].steps == 0) {
        return 0;
    }
    return (u32)step_start(&p->songs[track], p->songs[track].loop_step);
}

u32 music_loop_samples(const MusicPlayer *p, MusicTrack track) {
    if (track <= MUSIC_NONE || track >= MUSIC_COUNT || p->songs[track].steps == 0) {
        return 0;
    }
    const MusicSong *s = &p->songs[track];
    return (u32)(step_start(s, s->steps) - step_start(s, s->loop_step));
}

// ---- Voices ------------------------------------------------------------------------------------

static f32 per_sample(f32 seconds) {
    return expf(-1.0f / (seconds * RATE));
}

static f32 midi_hz(u32 note) {
    return 440.0f * powf(2.0f, ((f32)note - 69.0f) / 12.0f);
}

static void voice_on(MusicVoice *v, u32 note, f32 peak, f32 sustain, f32 decay_s, f32 release_s) {
    v->freq = midi_hz(note);
    v->peak = peak;
    v->sustain = sustain;
    v->decay = per_sample(decay_s);
    v->release = per_sample(release_s);
    v->attack = SFX_RATE / 250; // 4 ms: no click
    v->gate = true;
    v->held = 0.0f;
}

static f32 voice_env(MusicVoice *v) {
    if (v->attack > 0) {
        v->level += (v->peak - v->level) / (f32)v->attack;
        v->attack--;
    } else if (v->gate) {
        v->level = v->sustain + (v->level - v->sustain) * v->decay;
    } else {
        v->level *= v->release;
    }
    return v->level;
}

static f32 voice_pulse(MusicVoice *v, f32 freq) {
    v->phase += freq / RATE;
    v->phase -= floorf(v->phase);
    return (v->phase < v->duty ? 1.0f : -1.0f) - (2.0f * v->duty - 1.0f); // without its DC offset
}

static f32 voice_triangle(MusicVoice *v) {
    v->phase += v->freq / RATE;
    v->phase -= floorf(v->phase);
    return v->phase < 0.5f ? 4.0f * v->phase - 1.0f : 3.0f - 4.0f * v->phase;
}

// ---- Steps -------------------------------------------------------------------------------------

static f32 step_seconds(const MusicSong *s) {
    return 60.0f / ((f32)s->bpm * 4.0f);
}

static void bass_note(MusicPlayer *p, const MusicSong *s, const MusicChord *c, u32 offset, f32 steps) {
    voice_on(&p->bass, 36u + c->root + offset, 1.0f, 0.75f, 0.25f, 0.02f);
    p->bass_off = (u32)(steps * step_seconds(s) * RATE);
}

static void bass_step(MusicPlayer *p, const MusicSong *s, const MusicChord *c, u32 k) {
    switch (s->bass_style) {
    case BASS_WALK:
        if (k % 4 == 0) {
            static const u8 walk[4] = {0, 7, 12, 7};
            bass_note(p, s, c, walk[k / 4], 3.0f);
        }
        break;
    case BASS_BOUNCE:
        if (k % 2 == 0) {
            bass_note(p, s, c, k % 4 == 0 ? 0 : 12, 1.4f);
        }
        break;
    case BASS_DRIVE:
        if (k % 2 == 0) {
            bass_note(p, s, c, k == 14 ? 7 : (k == 6 ? 12 : 0), 1.2f);
        }
        break;
    default: // BASS_LONG
        if (k == 0) {
            bass_note(p, s, c, 0, 7.0f);
        } else if (k == 8) {
            bass_note(p, s, c, 7, 3.5f);
        } else if (k == 12) {
            bass_note(p, s, c, 12, 3.5f);
        }
        break;
    }
}

static u32 chord_note(const MusicChord *c, u32 index) {
    const u32 octave = index / c->count, tone = c->tones[index % c->count];
    u32 note = 60u + c->root + tone + 12u * octave;
    while (note > 84u) {
        note -= 12u;
    }
    return note;
}

static void accomp_step(MusicPlayer *p, const MusicSong *s, const MusicChord *c, u32 k) {
    switch (s->accomp_style) {
    case ACC_ARP: // up the chord, one tone per sixteenth
        p->accomp[0].duty = 0.125f;
        voice_on(&p->accomp[0], chord_note(c, k % (c->count + 1u)), 1.0f, 0.0f, 0.07f, 0.02f);
        break;
    case ACC_CHUNK: // the chord on the off-beats
        if (k % 4 == 2) {
            for (u32 v = 0; v < 3; v++) {
                p->accomp[v].duty = 0.25f;
                voice_on(&p->accomp[v], chord_note(c, v), 0.8f, 0.0f, 0.06f, 0.02f);
            }
        }
        break;
    default: // ACC_ROLL: up and down, one tone per eighth
        if (k % 2 == 0) {
            static const u8 roll[8] = {0, 1, 2, 3, 2, 1, 2, 3};
            p->accomp[0].duty = 0.25f;
            voice_on(&p->accomp[0], chord_note(c, roll[k / 2] % (c->count + 1u)), 0.9f, 0.0f, 0.3f, 0.05f);
        }
        break;
    }
}

static void start_step(MusicPlayer *p, const MusicSong *s, u32 step) {
    const u32 k = step % MUSIC_STEPS_PER_BAR;
    const u8 lead = s->lead[step];
    if (lead == MUSIC_REST) {
        p->lead.gate = false;
    } else if (lead != MUSIC_HOLD) {
        p->lead.duty = 0.25f;
        voice_on(&p->lead, lead, 1.0f, 0.55f, 0.35f, 0.05f);
    }
    const MusicChord *c = &s->chords[step];
    bass_step(p, s, c, k);
    accomp_step(p, s, c, k);
    const u8 d = s->drums[step];
    if (d & MUSIC_DRUM_KICK) {
        p->drums.kick_t = 0.0f;
    }
    if (d & MUSIC_DRUM_SNARE) {
        p->drums.snare_t = 0.0f;
    }
    if (d & (MUSIC_DRUM_HAT | MUSIC_DRUM_OPEN_HAT)) {
        p->drums.hat_t = 0.0f;
        p->drums.hat_len = (d & MUSIC_DRUM_OPEN_HAT) ? 0.14f : 0.035f;
    }
}

// ---- Rendering ---------------------------------------------------------------------------------

#define FADE_OUT_S 0.5f
#define FADE_IN_S 0.05f
#define VOLUME_SLEW_S 0.4f // time to go from silence to full volume
#define DRUM_OFF -1.0f
#define MIX_GAIN 0.8f // headroom: the channels together peak near full scale

static void restart(MusicPlayer *p, MusicTrack track) {
    p->current = track;
    p->volume = p->wanted_volume;
    p->pos = 0;
    p->step = UINT32_MAX;
    p->bass_off = 0;
    memset(&p->lead, 0, sizeof(p->lead));
    memset(&p->bass, 0, sizeof(p->bass));
    memset(p->accomp, 0, sizeof(p->accomp));
    p->drums.kick_t = p->drums.snare_t = p->drums.hat_t = DRUM_OFF;
    p->lead_lp = p->accomp_lp = 0.0f;
}

void music_play(MusicPlayer *p, MusicTrack track, f32 volume) {
    if (track < MUSIC_NONE || track >= MUSIC_COUNT) {
        track = MUSIC_NONE;
    }
    p->wanted = track;
    p->wanted_volume = clampf(volume, 0.0f, 1.0f);
    if (p->current == MUSIC_NONE && track != MUSIC_NONE) {
        p->gain = 0.0f;
        restart(p, track);
    }
}

void music_start(MusicPlayer *p, MusicTrack track, f32 volume) {
    if (track < MUSIC_NONE || track >= MUSIC_COUNT) {
        track = MUSIC_NONE;
    }
    p->wanted = track;
    p->wanted_volume = clampf(volume, 0.0f, 1.0f);
    restart(p, track);
    p->gain = 1.0f;
}

// Kick, snare and hat for one sample.
static f32 drums_sample(MusicDrums *d, f32 *hat_out) {
    const f32 dt = 1.0f / RATE;
    const f32 noise = rng_f32(&d->rng) * 2.0f - 1.0f;
    f32 out = 0.0f;
    if (d->kick_t >= 0.0f) {
        d->kick_phase += (45.0f + 120.0f * expf(-d->kick_t * 30.0f)) * dt;
        d->kick_phase -= floorf(d->kick_phase);
        out += sinf(TAU * d->kick_phase) * expf(-d->kick_t * 14.0f) * 0.42f;
        d->kick_t = d->kick_t + dt > 0.35f ? DRUM_OFF : d->kick_t + dt;
    }
    d->noise_lp += (noise - d->noise_lp) * 0.12f;
    if (d->snare_t >= 0.0f) {
        const f32 body = sinf(TAU * 185.0f * d->snare_t) * expf(-d->snare_t * 40.0f) * 0.5f;
        out += ((noise - d->noise_lp) + body) * expf(-d->snare_t * 22.0f) * 0.2f;
        d->snare_t = d->snare_t + dt > 0.3f ? DRUM_OFF : d->snare_t + dt;
    }
    d->hat_lp += (noise - d->hat_lp) * 0.6f;
    *hat_out = 0.0f;
    if (d->hat_t >= 0.0f) {
        *hat_out = (noise - d->hat_lp) * expf(-d->hat_t * 4.0f / d->hat_len) * 0.07f;
        d->hat_t = d->hat_t + dt > d->hat_len * 2.0f ? DRUM_OFF : d->hat_t + dt;
    }
    return out;
}

static void advance_song(MusicPlayer *p, const MusicSong *s) {
    u32 step = step_at(s, p->pos);
    if (step >= s->steps) {
        p->pos = step_start(s, s->loop_step);
        step = s->loop_step;
    }
    if (step != p->step) {
        p->step = step;
        start_step(p, s, step);
    }
    if (p->bass_off > 0 && --p->bass_off == 0) {
        p->bass.gate = false;
    }
    p->pos++;
}

// One stereo sample of the current song, before the master gain.
static void song_sample(MusicPlayer *p, f32 *left, f32 *right) {
    const MusicSong *s = &p->songs[p->current];
    advance_song(p, s);
    f32 lead_freq = p->lead.freq;
    p->lead.held += 1.0f / RATE;
    if (p->lead.held > 0.25f) {
        lead_freq *= 1.0f + 0.004f * sinf(TAU * 5.5f * p->lead.held);
    }
    const f32 lead = voice_pulse(&p->lead, lead_freq) * voice_env(&p->lead) * 0.2f;
    f32 accomp = 0.0f;
    for (u32 v = 0; v < 3; v++) {
        accomp += voice_pulse(&p->accomp[v], p->accomp[v].freq) * voice_env(&p->accomp[v]) * 0.085f;
    }
    p->lead_lp += (lead - p->lead_lp) * 0.45f;
    p->accomp_lp += (accomp - p->accomp_lp) * 0.35f;
    const f32 bass = voice_triangle(&p->bass) * voice_env(&p->bass) * 0.32f;
    f32 hat;
    const f32 drums = drums_sample(&p->drums, &hat);
    *left = p->lead_lp * 0.9f + p->accomp_lp * 0.6f + bass + drums + hat * 0.7f;
    *right = p->lead_lp * 0.7f + p->accomp_lp * 1.0f + bass + drums + hat;
}

static i16 to_i16(f32 x) {
    return (i16)(clampf(x, -1.0f, 1.0f) * 28000.0f);
}

void music_render(MusicPlayer *p, i16 *stereo, u32 frames) {
    const f32 fade_out = 1.0f / (FADE_OUT_S * RATE), fade_in = 1.0f / (FADE_IN_S * RATE);
    const f32 slew = 1.0f / (VOLUME_SLEW_S * RATE);
    for (u32 i = 0; i < frames; i++) {
        if (p->wanted != p->current) {
            p->gain -= fade_out;
            if (p->gain <= 0.0f) {
                p->gain = 0.0f;
                restart(p, p->wanted);
            }
        } else if (p->gain < 1.0f) {
            p->gain = MIN(1.0f, p->gain + fade_in);
        }
        if (p->volume < p->wanted_volume) {
            p->volume = MIN(p->wanted_volume, p->volume + slew);
        } else if (p->volume > p->wanted_volume) {
            p->volume = MAX(p->wanted_volume, p->volume - slew);
        }
        f32 left = 0.0f, right = 0.0f;
        if (p->current != MUSIC_NONE && p->songs[p->current].steps > 0) {
            song_sample(p, &left, &right);
        }
        const f32 g = p->gain * p->volume * MIX_GAIN;
        stereo[2 * i] = to_i16(left * g);
        stereo[2 * i + 1] = to_i16(right * g);
    }
}
