#include "app/sfx.h"

#include "game/crate.h"

#include "core/rng.h"
#include "game/weapon.h"

#include <math.h>

// ---- Synthesis ---------------------------------------------------------------------------------

typedef enum Wave { WAVE_SQUARE, WAVE_TRIANGLE, WAVE_SAW, WAVE_NOISE } Wave;

// One voice of a sound: a wave whose pitch slides from f0 to f1 (for noise: how often it changes value),
// with a quick attack and a decay to silence at the end.
typedef struct Layer {
    Wave wave;
    f32 start, dur; // seconds (dur 0 = unused)
    f32 f0, f1;     // Hz
    f32 volume;
    f32 lowpass;    // 0..1: 1 = unfiltered, lower = duller
    f32 vibrato;    // relative pitch wobble at 7 Hz
} Layer;

#define MAX_LAYERS 4

typedef struct SoundDef {
    const char *name;
    Layer layers[MAX_LAYERS];
} SoundDef;

#define SQ WAVE_SQUARE
#define TR WAVE_TRIANGLE
#define SW WAVE_SAW
#define NZ WAVE_NOISE

static const SoundDef SOUNDS[SFX_COUNT] = {
    [SFX_SHOT_PISTOL] = {"shot_pistol", {{NZ, 0, 0.09f, 9000, 2500, 0.7f, 0.6f, 0}, {SQ, 0, 0.06f, 900, 200, 0.35f, 1, 0}}},
    [SFX_SHOT_UZI] = {"shot_uzi", {{NZ, 0, 0.05f, 10000, 4000, 0.55f, 0.7f, 0}, {SQ, 0, 0.04f, 1300, 500, 0.25f, 1, 0}}},
    [SFX_THROW] = {"throw", {{TR, 0, 0.14f, 300, 700, 0.5f, 1, 0}, {NZ, 0, 0.1f, 3000, 6000, 0.12f, 0.3f, 0}}},
    [SFX_ROCKET] = {"rocket", {{NZ, 0, 0.45f, 6000, 1200, 0.6f, 0.35f, 0}, {SW, 0, 0.3f, 220, 110, 0.3f, 1, 0}}},
    [SFX_EXPLOSION] = {"explosion", {{NZ, 0, 0.8f, 5000, 600, 0.95f, 0.25f, 0}, {SQ, 0, 0.45f, 90, 35, 0.45f, 1, 0}}},
    [SFX_PUNCH] = {"punch", {{NZ, 0, 0.06f, 4000, 1500, 0.6f, 0.5f, 0}, {TR, 0, 0.09f, 200, 80, 0.6f, 1, 0}}},
    [SFX_HIT] = {"hit", {{SQ, 0, 0.08f, 260, 120, 0.4f, 1, 0}, {NZ, 0, 0.04f, 5000, 2000, 0.25f, 0.6f, 0}}},
    [SFX_CRATE_PICK] = {"crate_pick", {{TR, 0, 0.09f, 420, 760, 0.5f, 1, 0}}},
    [SFX_CRATE_PLACE] = {"crate_place", {{TR, 0, 0.09f, 240, 110, 0.6f, 1, 0}, {NZ, 0, 0.05f, 2500, 800, 0.4f, 0.4f, 0}}},
    [SFX_CRATE_DROP] = {"crate_drop", {{TR, 0, 0.12f, 520, 940, 0.3f, 1, 0}}},
    [SFX_BUILD] = {"build",
                   {{SQ, 0, 0.1f, 523, 523, 0.3f, 1, 0}, {SQ, 0.08f, 0.1f, 659, 659, 0.3f, 1, 0}, {SQ, 0.16f, 0.18f, 784, 784, 0.3f, 1, 0}}},
    [SFX_SPAWN] = {"spawn", {{TR, 0, 0.3f, 392, 784, 0.5f, 1, 0.03f}}},
    [SFX_DEATH] = {"death", {{SQ, 0, 0.45f, 440, 90, 0.35f, 1, 0}, {NZ, 0, 0.15f, 3000, 1000, 0.2f, 0.5f, 0}}},
    [SFX_SPLASH] = {"splash", {{NZ, 0, 0.35f, 7000, 1500, 0.6f, 0.3f, 0}, {TR, 0, 0.15f, 600, 200, 0.25f, 1, 0}}},
    [SFX_WEAPON_PICKUP] = {"weapon_pickup", {{SQ, 0, 0.06f, 660, 660, 0.3f, 1, 0}, {SQ, 0.06f, 0.08f, 990, 990, 0.3f, 1, 0}}},
    [SFX_EMPTY] = {"empty", {{SQ, 0, 0.05f, 160, 140, 0.35f, 1, 0}}},
    [SFX_ORDER] = {"order", {{TR, 0, 0.06f, 600, 600, 0.4f, 1, 0}, {TR, 0.06f, 0.07f, 800, 800, 0.4f, 1, 0}}},
    [SFX_BASE_DESTROYED] = {"base_destroyed", {{NZ, 0, 1.4f, 4000, 300, 1.0f, 0.2f, 0}, {SW, 0, 1.0f, 130, 30, 0.4f, 1, 0}}},
    [SFX_VICTORY] = {"victory",
                     {{SQ, 0, 0.12f, 523, 523, 0.3f, 1, 0},
                      {SQ, 0.12f, 0.12f, 659, 659, 0.3f, 1, 0},
                      {SQ, 0.24f, 0.12f, 784, 784, 0.3f, 1, 0},
                      {SQ, 0.36f, 0.4f, 1046, 1046, 0.3f, 1, 0.01f}}},
    [SFX_DEFEAT] = {"defeat",
                    {{TR, 0, 0.18f, 392, 392, 0.5f, 1, 0}, {TR, 0.18f, 0.18f, 330, 330, 0.5f, 1, 0}, {TR, 0.36f, 0.45f, 262, 250, 0.5f, 1, 0.02f}}},
    [SFX_MENU_MOVE] = {"menu_move", {{SQ, 0, 0.035f, 880, 880, 0.25f, 1, 0}}},
    [SFX_MENU_OK] = {"menu_ok", {{SQ, 0, 0.09f, 660, 990, 0.3f, 1, 0}}},
    [SFX_MENU_BACK] = {"menu_back", {{SQ, 0, 0.09f, 660, 440, 0.3f, 1, 0}}},
    [SFX_EXPLOSION_BIG] = {"explosion_big",
                           {{NZ, 0, 1.2f, 4000, 300, 1.0f, 0.2f, 0},
                            {SQ, 0, 0.7f, 70, 25, 0.5f, 1, 0},
                            {NZ, 0.05f, 0.5f, 9000, 3000, 0.35f, 0.5f, 0}}},
    [SFX_RICOCHET] = {"ricochet", {{SQ, 0, 0.12f, 2400, 3600, 0.18f, 1, 0.02f}, {NZ, 0, 0.03f, 9000, 6000, 0.3f, 0.7f, 0}}},
    [SFX_JUMP] = {"jump", {{SQ, 0, 0.08f, 300, 620, 0.28f, 0.6f, 0}}},
    [SFX_LAND] = {"land", {{NZ, 0, 0.06f, 1800, 500, 0.5f, 0.3f, 0}, {TR, 0, 0.06f, 140, 70, 0.5f, 1, 0}}},
    [SFX_CRATE_LAND] = {"crate_land", {{TR, 0, 0.14f, 120, 55, 0.7f, 1, 0}, {NZ, 0, 0.08f, 2200, 600, 0.45f, 0.35f, 0}}},
    [SFX_DUMMY_BREAK] = {"dummy_break",
                         {{NZ, 0, 0.25f, 3500, 900, 0.6f, 0.4f, 0},
                          {SQ, 0, 0.05f, 700, 300, 0.3f, 1, 0},
                          {TR, 0.05f, 0.2f, 300, 90, 0.4f, 1, 0}}},
    [SFX_WEAPON_DROP] = {"weapon_drop", {{SQ, 0, 0.05f, 1400, 900, 0.2f, 0.5f, 0}, {SQ, 0.05f, 0.06f, 1100, 800, 0.15f, 0.5f, 0}}},
    [SFX_SWITCH] = {"switch", {{TR, 0, 0.05f, 500, 500, 0.4f, 1, 0}, {TR, 0.04f, 0.08f, 750, 750, 0.4f, 1, 0}}},
    [SFX_ELIMINATED] = {"eliminated",
                        {{SW, 0, 0.2f, 330, 330, 0.25f, 0.5f, 0},
                         {SW, 0.2f, 0.2f, 311, 311, 0.25f, 0.5f, 0},
                         {SW, 0.4f, 0.5f, 294, 280, 0.25f, 0.5f, 0.02f}}},
    [SFX_ALARM] = {"alarm",
                   {{SQ, 0, 0.16f, 740, 740, 0.22f, 0.6f, 0},
                    {SQ, 0.16f, 0.16f, 554, 554, 0.22f, 0.6f, 0},
                    {SQ, 0.32f, 0.16f, 740, 740, 0.22f, 0.6f, 0},
                    {SQ, 0.48f, 0.2f, 554, 554, 0.22f, 0.6f, 0}}},
    [SFX_PAD_OPEN] = {"pad_open", {{TR, 0, 0.07f, 440, 660, 0.35f, 1, 0}, {TR, 0.05f, 0.07f, 660, 880, 0.3f, 1, 0}}},
};

const char *sfx_name(SfxId id) {
    return id >= 0 && id < SFX_COUNT ? SOUNDS[id].name : "?";
}

static f32 wave_value(Wave w, f32 phase) {
    switch (w) {
    case WAVE_SQUARE:
        return phase < 0.5f ? 1.0f : -1.0f;
    case WAVE_TRIANGLE:
        return phase < 0.5f ? 4.0f * phase - 1.0f : 3.0f - 4.0f * phase;
    case WAVE_SAW:
        return 2.0f * phase - 1.0f;
    default:
        return 0.0f;
    }
}

// Adds the layer to `out` (saturating).
static void render_layer(const Layer *l, u32 seed, i16 *out, u32 cap) {
    Rng rng;
    rng_seed(&rng, seed, 11);
    const u32 first = (u32)(l->start * (f32)SFX_RATE);
    const u32 count = (u32)(l->dur * (f32)SFX_RATE);
    const u32 attack = SFX_RATE / 300; // ~3 ms: no click at the start
    f32 phase = 0.0f, held = 0.0f, filtered = 0.0f;
    for (u32 i = 0; i < count && first + i < cap; i++) {
        const f32 t = (f32)i / (f32)count;
        f32 freq = l->f0 * powf(l->f1 / l->f0, t);
        if (l->vibrato > 0.0f) {
            freq *= 1.0f + l->vibrato * sinf(2.0f * 3.14159265f * 7.0f * (f32)i / (f32)SFX_RATE);
        }
        const f32 prev = phase;
        phase += freq / (f32)SFX_RATE;
        phase -= floorf(phase);
        f32 v;
        if (l->wave == WAVE_NOISE) {
            if (phase < prev || i == 0) { // a new random value at each period
                held = rng_f32(&rng) * 2.0f - 1.0f;
            }
            v = held;
        } else {
            v = wave_value(l->wave, phase);
        }
        filtered += (v - filtered) * l->lowpass;
        const f32 decay = (1.0f - t) * (1.0f - t);
        const f32 env = (i < attack ? (f32)i / (f32)attack : 1.0f) * decay;
        const f32 sum = (f32)out[first + i] + filtered * env * l->volume * 26000.0f;
        out[first + i] = (i16)clampf(sum, -32767.0f, 32767.0f);
    }
}

u32 sfx_synthesize(SfxId id, i16 *out, u32 cap) {
    if (id < 0 || id >= SFX_COUNT || cap == 0) {
        return 0;
    }
    const SoundDef *def = &SOUNDS[id];
    u32 length = 0;
    for (int k = 0; k < MAX_LAYERS; k++) {
        const Layer *l = &def->layers[k];
        if (l->dur > 0.0f) {
            const u32 end = (u32)((l->start + l->dur) * (f32)SFX_RATE);
            length = end > length ? end : length;
        }
    }
    length = MIN(MIN(length, cap), (u32)SFX_MAX_SAMPLES);
    for (u32 i = 0; i < length; i++) {
        out[i] = 0;
    }
    for (int k = 0; k < MAX_LAYERS; k++) {
        if (def->layers[k].dur > 0.0f) {
            render_layer(&def->layers[k], (u32)id * 16u + (u32)k + 1u, out, length);
        }
    }
    return length;
}

// ---- Cues --------------------------------------------------------------------------------------

typedef struct CueRule {
    u32 gap;          // minimum ticks between two plays
    f32 volume;
    bool positional;  // fades with the distance to the listener
} CueRule;

static const CueRule RULES[SFX_COUNT] = {
    [SFX_SHOT_PISTOL] = {2, 0.6f, true},  [SFX_SHOT_UZI] = {3, 0.45f, true},   [SFX_THROW] = {4, 0.5f, true},
    [SFX_ROCKET] = {4, 0.7f, true},       [SFX_EXPLOSION] = {3, 1.0f, true},   [SFX_PUNCH] = {4, 0.6f, true},
    [SFX_HIT] = {5, 0.5f, true},          [SFX_CRATE_PICK] = {3, 0.5f, true},  [SFX_CRATE_PLACE] = {3, 0.6f, true},
    [SFX_CRATE_DROP] = {20, 0.3f, true},  [SFX_BUILD] = {6, 0.7f, true},       [SFX_SPAWN] = {6, 0.6f, true},
    [SFX_DEATH] = {6, 0.7f, true},        [SFX_SPLASH] = {6, 0.6f, true},      [SFX_WEAPON_PICKUP] = {4, 0.6f, true},
    [SFX_EMPTY] = {6, 0.5f, true},        [SFX_ORDER] = {4, 0.6f, false},      [SFX_BASE_DESTROYED] = {30, 1.0f, false},
    [SFX_VICTORY] = {60, 0.8f, false},    [SFX_DEFEAT] = {60, 0.8f, false},    [SFX_MENU_MOVE] = {1, 0.5f, false},
    [SFX_MENU_OK] = {1, 0.6f, false},     [SFX_MENU_BACK] = {1, 0.6f, false},  [SFX_EXPLOSION_BIG] = {3, 1.0f, true},
    [SFX_RICOCHET] = {5, 0.35f, true},    [SFX_JUMP] = {4, 0.3f, true},        [SFX_LAND] = {4, 0.35f, true},
    [SFX_CRATE_LAND] = {4, 0.5f, true},   [SFX_DUMMY_BREAK] = {6, 0.7f, true}, [SFX_WEAPON_DROP] = {4, 0.4f, true},
    [SFX_SWITCH] = {4, 0.5f, false},      [SFX_ELIMINATED] = {30, 0.8f, false}, [SFX_ALARM] = {60, 0.55f, false},
    [SFX_PAD_OPEN] = {6, 0.45f, false},
};

#define BIG_EXPLOSION_RADIUS 48 // px: rockets and strong grenades sound bigger

#define HEAR_FULL 220.0f  // closer than this: full volume
#define HEAR_RANGE 760.0f // fades out until this distance
#define PAN_RANGE 420.0f

void sfx_push(SfxCueList *list, SfxId id, f32 volume, f32 pan) {
    for (u32 i = 0; i < list->count; i++) {
        if (list->items[i].id == id) { // one per sound and frame: the loudest
            if (volume > list->items[i].volume) {
                list->items[i] = (SfxCue){id, volume, pan};
            }
            return;
        }
    }
    if (list->count < SFX_MAX_CUES) {
        list->items[list->count++] = (SfxCue){id, volume, pan};
    }
}

static SfxId shot_sound(i32 weapon) {
    switch (weapon) {
    case WEAPON_UZI:
        return SFX_SHOT_UZI;
    case WEAPON_GRENADES:
    case WEAPON_STRONG_GRENADES:
        return SFX_THROW;
    case WEAPON_ROCKET_LAUNCHER:
        return SFX_ROCKET;
    default:
        return SFX_SHOT_PISTOL;
    }
}

// The sound of an event, or SFX_COUNT for none.
static SfxId event_sound(const SfxState *state, const EventQueue *events, const GameEvent *e) {
    switch (e->type) {
    case EV_SHOT:
        return shot_sound(e->a);
    case EV_PUNCH:
        return SFX_PUNCH;
    case EV_EXPLOSION:
        return e->a >= BIG_EXPLOSION_RADIUS ? SFX_EXPLOSION_BIG : SFX_EXPLOSION;
    case EV_PROJECTILE_HIT:
        return e->b == 1 ? SFX_RICOCHET : SFX_COUNT; // on a target, the hit sound plays
    case EV_DAMAGE:
        return SFX_HIT;
    case EV_CRATE_PICKED_UP:
        return SFX_CRATE_PICK;
    case EV_CRATE_PLACED:
        return SFX_CRATE_PLACE;
    case EV_CRATE_THROWN:
        return SFX_THROW;
    case EV_CRATE_SPAWNED:
        return SFX_CRATE_DROP;
    case EV_CRATE_SETTLED:
        return SFX_CRATE_LAND;
    case EV_CRATE_DESTROYED:
    case EV_PLAYER_RESPAWNED:
        return SFX_SPLASH;
    case EV_PAD_BUILT:
        return SFX_BUILD;
    case EV_SOLDIER_SPAWNED:
        return SFX_SPAWN;
    case EV_SOLDIER_DIED:
        return SFX_DEATH;
    case EV_WEAPON_PICKED_UP:
        return SFX_WEAPON_PICKUP;
    case EV_CRATE_SALVAGED: // the pickup chime when it gave something, the sound of wood breaking when it did not
        return e->a == SALVAGE_NOTHING ? SFX_DUMMY_BREAK : SFX_WEAPON_PICKUP;
    case EV_WEAPON_EMPTY:
        return SFX_EMPTY;
    case EV_WEAPON_DROPPED:
        return SFX_WEAPON_DROP;
    case EV_DUMMY_DESTROYED:
        return SFX_DUMMY_BREAK;
    case EV_JUMP: // footwork of the whole squad would drown the fight: only the followed soldier's
        return e->b == state->focus_soldier ? SFX_JUMP : SFX_COUNT;
    case EV_LANDED:
        return e->b == state->focus_soldier ? SFX_LAND : SFX_COUNT;
    case EV_CONTROL_CHANGED:
        return SFX_SWITCH;
    case EV_BASE_ATTACKED:
        return state->human_team >= 0 && e->a == state->human_team ? SFX_ALARM : SFX_COUNT;
    case EV_TEAM_ELIMINATED: // the end of the match has its own sound
        return events_count_of(events, EV_MATCH_OVER) == 0 ? SFX_ELIMINATED : SFX_COUNT;
    case EV_ORDER_CHANGED:
        return SFX_ORDER;
    case EV_BASE_DESTROYED:
        return SFX_BASE_DESTROYED;
    case EV_MATCH_OVER:
        return (state->human_team < 0 || e->a == state->human_team) ? SFX_VICTORY : SFX_DEFEAT;
    default:
        return SFX_COUNT;
    }
}

void sfx_play_ui(SfxState *state, SfxId id, SfxCueList *out) {
    if (id < 0 || id >= SFX_COUNT || state->cooldown[id] > 0) {
        return;
    }
    sfx_push(out, id, RULES[id].volume, 0.0f);
    state->cooldown[id] = RULES[id].gap;
}

void sfx_consume(SfxState *state, const EventQueue *events, Vec2 listener, SfxCueList *out) {
    for (int id = 0; id < SFX_COUNT; id++) {
        if (state->cooldown[id] > 0) {
            state->cooldown[id]--;
        }
    }
    for (u32 i = 0; i < events->count; i++) {
        const GameEvent *e = &events->items[i];
        const SfxId id = event_sound(state, events, e);
        if (id == SFX_COUNT || state->cooldown[id] > 0) {
            continue;
        }
        const CueRule *rule = &RULES[id];
        f32 volume = rule->volume, pan = 0.0f;
        if (rule->positional) {
            const f32 dx = e->pos.x - listener.x, dy = e->pos.y - listener.y;
            const f32 dist = sqrtf(dx * dx + dy * dy);
            volume *= clampf(1.0f - (dist - HEAR_FULL) / (HEAR_RANGE - HEAR_FULL), 0.0f, 1.0f);
            pan = clampf(dx / PAN_RANGE, -1.0f, 1.0f);
        }
        if (volume <= 0.01f) {
            continue;
        }
        sfx_push(out, id, volume, pan);
        state->cooldown[id] = rule->gap;
    }
}
