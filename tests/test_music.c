// Placeholder music: every song parses, sounds, loops, and switching songs fades instead of clicking.
#include "test.h"

#include "app/front.h"
#include "app/music.h"
#include "test_helpers.h"

#include <string.h>

static MusicPlayer g_player, g_other;
#define CHUNK (SFX_RATE / 10)
static i16 g_buf[CHUNK * 2];

typedef struct Loudness {
    i32 peak;
    f64 rms;
} Loudness;

static Loudness measure(MusicPlayer *p, u32 frames) {
    Loudness l = {0, 0.0};
    f64 sum = 0.0;
    u32 done = 0;
    while (done < frames) {
        const u32 n = MIN((u32)CHUNK, frames - done);
        music_render(p, g_buf, n);
        for (u32 i = 0; i < n * 2; i++) {
            const i32 v = g_buf[i] < 0 ? -(i32)g_buf[i] : (i32)g_buf[i];
            l.peak = MAX(l.peak, v);
            sum += (f64)g_buf[i] * (f64)g_buf[i];
        }
        done += n;
    }
    l.rms = frames > 0 ? sqrt(sum / (f64)(frames * 2)) : 0.0;
    return l;
}

static void test_every_song_parses_and_has_a_loop(void) {
    CHECK(music_init(&g_player));
    CHECK(g_player.error[0] == '\0');
    for (int t = MUSIC_TITLE; t < MUSIC_COUNT; t++) {
        const MusicSong *s = &g_player.songs[t];
        CHECK(s->steps > 0 && s->steps % MUSIC_STEPS_PER_BAR == 0 && s->loop_step < s->steps);
        CHECK(music_loop_samples(&g_player, (MusicTrack)t) > (u32)SFX_RATE * 10); // more than 10 s before it repeats
        CHECK(strcmp(music_name((MusicTrack)t), "?") != 0);
        // The melody starts somewhere and only uses notes a pulse lead plays well.
        u32 notes = 0;
        for (u32 k = 0; k < s->steps; k++) {
            if (s->lead[k] != MUSIC_HOLD && s->lead[k] != MUSIC_REST) {
                notes++;
                CHECK(s->lead[k] >= 52 && s->lead[k] <= 91); // E3..G6
            }
        }
        CHECK(notes >= s->steps / 8);
    }
    CHECK(music_intro_samples(&g_player, MUSIC_TITLE) > 0); // the title song has an intro
    CHECK(music_loop_samples(&g_player, MUSIC_NONE) == 0);
}

static void test_every_song_is_audible_without_clipping(void) {
    CHECK(music_init(&g_player));
    for (int t = MUSIC_TITLE; t < MUSIC_COUNT; t++) {
        music_start(&g_player, (MusicTrack)t, 1.0f);
        const Loudness l = measure(&g_player, SFX_RATE * 8);
        if (l.rms < 2500.0 || l.peak >= 26000) {
            fprintf(stderr, "  %s: peak %d rms %.0f\n", music_name((MusicTrack)t), l.peak, l.rms);
        }
        CHECK(l.rms > 2500.0);
        CHECK(l.peak < 26000); // headroom: never pinned to the limit
    }
}

static void test_rendering_is_deterministic(void) {
    CHECK(music_init(&g_player));
    CHECK(music_init(&g_other));
    music_start(&g_player, MUSIC_COVE, 1.0f);
    music_start(&g_other, MUSIC_COVE, 1.0f);
    static i16 a[CHUNK * 2], b[CHUNK * 2];
    for (int k = 0; k < 20; k++) {
        music_render(&g_player, a, CHUNK);
        music_render(&g_other, b, CHUNK);
        CHECK(memcmp(a, b, sizeof(a)) == 0);
    }
}

static void test_the_song_loops_back_after_its_intro(void) {
    CHECK(music_init(&g_player));
    music_start(&g_player, MUSIC_TITLE, 1.0f);
    const u32 intro = music_intro_samples(&g_player, MUSIC_TITLE), loop = music_loop_samples(&g_player, MUSIC_TITLE);
    measure(&g_player, intro + loop);
    CHECK(g_player.step == g_player.songs[MUSIC_TITLE].steps - 1); // the very last step
    measure(&g_player, 1);
    CHECK(g_player.step == g_player.songs[MUSIC_TITLE].loop_step); // the intro is not played again
    CHECK(g_player.pos == (u64)intro + 1u);
    const Loudness again = measure(&g_player, SFX_RATE * 2);
    CHECK(again.rms > 2500.0);
}

static void test_switching_fades_out_then_starts_the_new_song(void) {
    CHECK(music_init(&g_player));
    music_play(&g_player, MUSIC_TITLE, 1.0f); // from silence: at once
    CHECK(g_player.current == MUSIC_TITLE);
    measure(&g_player, SFX_RATE);
    music_play(&g_player, MUSIC_COVE, 1.0f);
    CHECK(g_player.current == MUSIC_TITLE); // still the old one, fading
    measure(&g_player, SFX_RATE / 4);
    CHECK(g_player.current == MUSIC_TITLE && g_player.gain > 0.3f && g_player.gain < 0.7f);
    measure(&g_player, SFX_RATE / 2);
    CHECK(g_player.current == MUSIC_COVE && g_player.pos < (u64)SFX_RATE / 2); // from its beginning
    measure(&g_player, SFX_RATE / 10);
    CHECK(g_player.gain == 1.0f);
    // To silence: it fades, then nothing.
    music_play(&g_player, MUSIC_NONE, 1.0f);
    const Loudness fading = measure(&g_player, SFX_RATE / 4);
    CHECK(fading.rms > 300.0);
    measure(&g_player, SFX_RATE / 2);
    CHECK(g_player.current == MUSIC_NONE);
    CHECK(measure(&g_player, SFX_RATE / 2).peak == 0);
}

static void test_the_same_song_only_changes_its_volume(void) {
    CHECK(music_init(&g_player));
    music_start(&g_player, MUSIC_ARENA, 1.0f);
    const Loudness loud = measure(&g_player, SFX_RATE * 4);
    const u64 pos = g_player.pos;
    music_play(&g_player, MUSIC_ARENA, MUSIC_PAUSE_VOLUME);
    measure(&g_player, SFX_RATE / 2); // the volume glides down
    CHECK(g_player.current == MUSIC_ARENA && g_player.pos == pos + (u64)SFX_RATE / 2u); // no restart
    CHECK(g_player.volume == MUSIC_PAUSE_VOLUME);
    const Loudness quiet = measure(&g_player, SFX_RATE * 4);
    CHECK(quiet.rms < loud.rms * 0.5);
    music_play(&g_player, MUSIC_COUNT, 1.0f); // nonsense: silence
    CHECK(g_player.wanted == MUSIC_NONE);
}

static Front g_front;

static void tick(Front *f, InputFrame *in, u32 mask) {
    input_frame_update(in, mask);
    front_tick(f, in);
}

static void test_each_screen_has_its_music(void) {
    const FrontConfig cfg = {.assets_dir = CS_ASSET_DIR, .seed = 3};
    CHECK(front_init(&g_front, &cfg));
    f32 volume = 0.0f;
    CHECK(front_music(&g_front, &volume) == MUSIC_TITLE && volume == 1.0f);
    InputFrame in = {0};
    tick(&g_front, &in, KEY(ACT_CONFIRM)); // setup
    tick(&g_front, &in, 0);
    CHECK(g_front.screen == SCREEN_SETUP && front_music(&g_front, &volume) == MUSIC_TITLE);
    tick(&g_front, &in, KEY(ACT_CONFIRM)); // La Crique
    tick(&g_front, &in, 0);
    CHECK(g_front.screen == SCREEN_PLAY && front_music(&g_front, &volume) == MUSIC_COVE && volume == 1.0f);
    tick(&g_front, &in, KEY(ACT_PAUSE));
    CHECK(g_front.screen == SCREEN_PAUSE && front_music(&g_front, &volume) == MUSIC_COVE);
    CHECK(volume == MUSIC_PAUSE_VOLUME);
    tick(&g_front, &in, 0);
    tick(&g_front, &in, KEY(ACT_PAUSE)); // back to the match
    tick(&g_front, &in, 0);
    g_front.sim.gs.match.over = true; // the end: silence for the jingle
    CHECK(front_music(&g_front, &volume) == MUSIC_NONE);
    for (int i = 0; i < 60 * 4 && g_front.screen == SCREEN_PLAY; i++) {
        tick(&g_front, &in, 0);
    }
    CHECK(g_front.screen == SCREEN_RESULTS && front_music(&g_front, &volume) == MUSIC_RESULTS);
    front_free(&g_front);
}

static void test_levels_have_their_music(void) {
    CHECK(front_level_music("assets/levels/beach.lvl") == MUSIC_COVE);
    CHECK(front_level_music("/x/ai_arena.lvl") == MUSIC_ARENA);
    CHECK(front_level_music("test_arena.lvl") == MUSIC_ARENA);
    CHECK(front_level_music("elsewhere/custom.lvl") == MUSIC_COVE);
    CHECK(front_level_music(NULL) == MUSIC_COVE);
}

int main(void) {
    RUN_TEST(test_every_song_parses_and_has_a_loop);
    RUN_TEST(test_every_song_is_audible_without_clipping);
    RUN_TEST(test_rendering_is_deterministic);
    RUN_TEST(test_the_song_loops_back_after_its_intro);
    RUN_TEST(test_switching_fades_out_then_starts_the_new_song);
    RUN_TEST(test_the_same_song_only_changes_its_volume);
    RUN_TEST(test_each_screen_has_its_music);
    RUN_TEST(test_levels_have_their_music);
    TEST_MAIN_END();
}
