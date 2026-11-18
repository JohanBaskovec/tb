// Placeholder sounds: synthesis, and which events make which sound, how loud and on which side.
#include "test.h"

#include "app/front.h"
#include "app/sfx.h"
#include "game/weapon.h"
#include "test_helpers.h"

static i16 g_buf[SFX_MAX_SAMPLES];
static i16 g_buf2[SFX_MAX_SAMPLES];

static void test_every_sound_is_synthesized_audible_and_ends_quietly(void) {
    for (int id = 0; id < SFX_COUNT; id++) {
        const u32 n = sfx_synthesize((SfxId)id, g_buf, SFX_MAX_SAMPLES);
        i32 peak = 0;
        for (u32 i = 0; i < n; i++) {
            peak = MAX(peak, g_buf[i] < 0 ? -(i32)g_buf[i] : (i32)g_buf[i]);
        }
        if (n < SFX_RATE / 50 || peak < 2000) {
            fprintf(stderr, "  %s: %u samples, peak %d\n", sfx_name((SfxId)id), n, peak);
        }
        CHECK(n >= SFX_RATE / 50 && n <= SFX_MAX_SAMPLES); // at least 20 ms
        CHECK(peak >= 2000);
        i32 tail = 0; // the last millisecond is close to silence: no click at the end
        for (u32 i = n - SFX_RATE / 1000; i < n; i++) {
            tail = MAX(tail, g_buf[i] < 0 ? -(i32)g_buf[i] : (i32)g_buf[i]);
        }
        CHECK(tail < 1500);
        CHECK(strcmp(sfx_name((SfxId)id), "?") != 0);
    }
}

static void test_synthesis_is_deterministic_and_respects_the_buffer(void) {
    const u32 a = sfx_synthesize(SFX_EXPLOSION, g_buf, SFX_MAX_SAMPLES);
    const u32 b = sfx_synthesize(SFX_EXPLOSION, g_buf2, SFX_MAX_SAMPLES);
    CHECK(a == b && memcmp(g_buf, g_buf2, a * sizeof(i16)) == 0);
    CHECK(sfx_synthesize(SFX_EXPLOSION, g_buf, 100) == 100);
    CHECK(sfx_synthesize(SFX_COUNT, g_buf, SFX_MAX_SAMPLES) == 0);
}

static SfxCueList consume_one(SfxState *st, EventType type, Vec2 pos, i32 a, i32 b, Vec2 listener) {
    EventQueue q = {0};
    events_push(&q, type, pos, a, b);
    SfxCueList out = {0};
    sfx_consume(st, &q, listener, &out);
    return out;
}

static void test_events_make_the_matching_sounds(void) {
    SfxState st = {.human_team = 0};
    const Vec2 here = vec2(100.0f, 100.0f);
    SfxCueList out = consume_one(&st, EV_SHOT, here, WEAPON_PISTOL, 1, here);
    CHECK(out.count == 1 && out.items[0].id == SFX_SHOT_PISTOL);
    out = consume_one(&st, EV_SHOT, here, WEAPON_ROCKET_LAUNCHER, 1, here);
    CHECK(out.count == 1 && out.items[0].id == SFX_ROCKET);
    out = consume_one(&st, EV_EXPLOSION, here, 40, WEAPON_GRENADES, here);
    CHECK(out.count == 1 && out.items[0].id == SFX_EXPLOSION);
    out = consume_one(&st, EV_PAD_BUILT, here, 0, 0, here);
    CHECK(out.count == 1 && out.items[0].id == SFX_BUILD);
    out = consume_one(&st, EV_CRATE_DESTROYED, here, 0, 0, here);
    CHECK(out.count == 1 && out.items[0].id == SFX_SPLASH);
    out = consume_one(&st, EV_CRATE_SETTLED, here, 0, 0, here);
    CHECK(out.count == 1 && out.items[0].id == SFX_CRATE_LAND);
    out = consume_one(&st, EV_CRATE_CONSUMED, here, 0, 0, here); // the build sound covers it
    CHECK(out.count == 0);
    out = consume_one(&st, EV_EXPLOSION, here, 64, WEAPON_STRONG_GRENADES, here);
    CHECK(out.count == 1 && out.items[0].id == SFX_EXPLOSION_BIG);
    out = consume_one(&st, EV_PROJECTILE_HIT, here, WEAPON_PISTOL, 1, here); // into a wall
    CHECK(out.count == 1 && out.items[0].id == SFX_RICOCHET);
    out = consume_one(&st, EV_PROJECTILE_HIT, here, WEAPON_PISTOL, 0, here); // into a target: its hit sound
    CHECK(out.count == 0);
    out = consume_one(&st, EV_JUMP, here, 0, 0, here); // soldier 0: the followed one
    CHECK(out.count == 1 && out.items[0].id == SFX_JUMP);
    out = consume_one(&st, EV_LANDED, here, 0, 0, here);
    CHECK(out.count == 1 && out.items[0].id == SFX_LAND);
    out = consume_one(&st, EV_JUMP, here, 0, 3, here); // another soldier: not heard
    CHECK(out.count == 0);
    st.focus_soldier = -1;
    out = consume_one(&st, EV_LANDED, here, 0, 0, here);
    CHECK(out.count == 0);
    out = consume_one(&st, EV_DUMMY_DESTROYED, here, 0, 0, here);
    CHECK(out.count == 1 && out.items[0].id == SFX_DUMMY_BREAK);
    out = consume_one(&st, EV_WEAPON_DROPPED, here, WEAPON_UZI, 0, here);
    CHECK(out.count == 1 && out.items[0].id == SFX_WEAPON_DROP);
    out = consume_one(&st, EV_CONTROL_CHANGED, here, 1, 0, here);
    CHECK(out.count == 1 && out.items[0].id == SFX_SWITCH);
}

static void test_far_events_are_quieter_then_silent_and_panned(void) {
    SfxState st = {.human_team = 0};
    const Vec2 me = vec2(1000.0f, 300.0f);
    SfxCueList near = consume_one(&st, EV_PUNCH, vec2(1050.0f, 300.0f), 0, 0, me);
    st = (SfxState){.human_team = 0};
    SfxCueList mid = consume_one(&st, EV_PUNCH, vec2(1500.0f, 300.0f), 0, 0, me);
    st = (SfxState){.human_team = 0};
    SfxCueList far = consume_one(&st, EV_PUNCH, vec2(3000.0f, 300.0f), 0, 0, me);
    st = (SfxState){.human_team = 0};
    SfxCueList left = consume_one(&st, EV_PUNCH, vec2(700.0f, 300.0f), 0, 0, me);
    CHECK(near.count == 1 && mid.count == 1 && far.count == 0);
    CHECK(mid.items[0].volume < near.items[0].volume);
    CHECK(mid.items[0].pan > 0.5f && left.items[0].pan < -0.5f);
    // Not positional: as loud anywhere.
    st = (SfxState){.human_team = 0};
    SfxCueList base = consume_one(&st, EV_BASE_DESTROYED, vec2(9000.0f, 0.0f), 1, 0, me);
    CHECK(base.count == 1 && base.items[0].volume > 0.9f && base.items[0].pan == 0.0f);
}

static void test_a_burst_makes_one_sound_and_the_gap_is_kept(void) {
    SfxState st = {.human_team = 0};
    const Vec2 here = vec2(0.0f, 0.0f);
    EventQueue q = {0};
    for (int i = 0; i < 10; i++) {
        events_push(&q, EV_DAMAGE, here, 10, 1);
    }
    SfxCueList out = {0};
    sfx_consume(&st, &q, here, &out);
    CHECK(out.count == 1);
    out.count = 0;
    sfx_consume(&st, &q, here, &out); // next tick: still within the gap
    CHECK(out.count == 0);
    EventQueue none = {0};
    for (int i = 0; i < 10; i++) {
        sfx_consume(&st, &none, here, &out);
    }
    sfx_consume(&st, &q, here, &out);
    CHECK(out.count == 1);
}

static void test_the_end_of_a_match_sounds_like_its_result(void) {
    const Vec2 here = vec2(0.0f, 0.0f);
    SfxState st = {.human_team = 0};
    CHECK(consume_one(&st, EV_MATCH_OVER, here, 0, 0, here).items[0].id == SFX_VICTORY);
    st = (SfxState){.human_team = 0};
    CHECK(consume_one(&st, EV_MATCH_OVER, here, 1, 0, here).items[0].id == SFX_DEFEAT);
    st = (SfxState){.human_team = 0};
    CHECK(consume_one(&st, EV_MATCH_OVER, here, -1, 0, here).items[0].id == SFX_DEFEAT); // a draw is no win
    st = (SfxState){.human_team = -1}; // AI against AI: always a fanfare
    CHECK(consume_one(&st, EV_MATCH_OVER, here, 1, 0, here).items[0].id == SFX_VICTORY);
}

static void test_only_the_players_base_sounds_the_alarm(void) {
    const Vec2 here = vec2(0.0f, 0.0f);
    SfxState st = {.human_team = 0};
    CHECK(consume_one(&st, EV_BASE_ATTACKED, here, 0, 1, here).items[0].id == SFX_ALARM);
    st = (SfxState){.human_team = 0};
    CHECK(consume_one(&st, EV_BASE_ATTACKED, here, 1, 0, here).count == 0);
    st = (SfxState){.human_team = -1}; // spectator: no base of its own
    CHECK(consume_one(&st, EV_BASE_ATTACKED, here, 0, 1, here).count == 0);
}

static void test_an_elimination_sounds_unless_it_ends_the_match(void) {
    const Vec2 here = vec2(0.0f, 0.0f);
    SfxState st = {.human_team = 0};
    CHECK(consume_one(&st, EV_TEAM_ELIMINATED, here, 2, 0, here).items[0].id == SFX_ELIMINATED);
    st = (SfxState){.human_team = 0};
    EventQueue q = {0};
    events_push(&q, EV_TEAM_ELIMINATED, here, 1, 0);
    events_push(&q, EV_MATCH_OVER, here, 0, 0);
    SfxCueList out = {0};
    sfx_consume(&st, &q, here, &out);
    CHECK(out.count == 1 && out.items[0].id == SFX_VICTORY);
}

static void test_ui_sounds_keep_their_gap(void) {
    SfxState st = {0};
    SfxCueList out = {0};
    sfx_play_ui(&st, SFX_PAD_OPEN, &out);
    sfx_play_ui(&st, SFX_PAD_OPEN, &out);
    CHECK(out.count == 1 && out.items[0].id == SFX_PAD_OPEN && out.items[0].pan == 0.0f);
    sfx_play_ui(&st, SFX_COUNT, &out);
    CHECK(out.count == 1);
}

static void test_the_cue_list_merges_and_caps(void) {
    SfxCueList list = {0};
    sfx_push(&list, SFX_HIT, 0.2f, 0.0f);
    sfx_push(&list, SFX_HIT, 0.8f, 0.5f);
    sfx_push(&list, SFX_HIT, 0.4f, 0.0f);
    CHECK(list.count == 1 && list.items[0].volume == 0.8f && list.items[0].pan == 0.5f);
    for (int id = 0; id < SFX_COUNT; id++) {
        sfx_push(&list, (SfxId)id, 0.5f, 0.0f);
    }
    CHECK(list.count == (u32)MIN(SFX_COUNT, SFX_MAX_CUES));
}

static Front g_front;

static void test_menus_and_matches_queue_their_sounds(void) {
    const FrontConfig cfg = {.assets_dir = CS_ASSET_DIR, .seed = 3};
    CHECK(front_init(&g_front, &cfg));
    InputFrame in = {0};
    input_frame_update(&in, KEY(ACT_DOWN));
    front_tick(&g_front, &in);
    CHECK(g_front.cues.count == 1 && g_front.cues.items[0].id == SFX_MENU_MOVE);
    g_front.cues.count = 0;
    input_frame_update(&in, 0);
    front_tick(&g_front, &in);
    input_frame_update(&in, KEY(ACT_UP)); // back on "play"
    front_tick(&g_front, &in);
    input_frame_update(&in, 0);
    front_tick(&g_front, &in);
    g_front.cues.count = 0;
    input_frame_update(&in, KEY(ACT_CONFIRM));
    front_tick(&g_front, &in);
    CHECK(g_front.screen == SCREEN_SETUP);
    CHECK(g_front.cues.count == 1 && g_front.cues.items[0].id == SFX_MENU_OK);
    input_frame_update(&in, 0);
    front_tick(&g_front, &in);
    input_frame_update(&in, KEY(ACT_CONFIRM));
    front_tick(&g_front, &in); // the match starts
    Sim *sim = front_sim(&g_front);
    CHECK(sim != NULL && sim->sfx.human_team == 0);
    // The player's soldier picks a fight with the air: a punch is heard.
    for (int i = 0; i < 4; i++) {
        input_frame_update(&in, i % 2 == 0 ? KEY(ACT_FIRE) : 0);
        front_tick(&g_front, &in);
    }
    bool punch = false;
    for (u32 i = 0; i < sim->cues.count; i++) {
        punch |= sim->cues.items[i].id == SFX_PUNCH;
    }
    CHECK(punch);
    front_free(&g_front);
}

static Sim g_sim;

// AI against AI on La Crique for a minute, heard from the camera: busy, but not a wall of noise.
static void test_a_match_is_not_a_wall_of_noise(void) {
    const SimConfig cfg = {.level_path = CS_ASSET_DIR "/levels/beach.lvl", .seed = 7, .ai_teams = 1, .ai_opponents = true,
                           .start_squad = 3};
    char err[256] = "";
    CHECK(sim_init(&g_sim, &cfg, err, sizeof(err)));
    u32 per_sound[SFX_COUNT] = {0}, total = 0, busiest = 0;
    InputFrame in = {0};
    for (int second = 0; second < 60; second++) {
        u32 this_second = 0;
        for (int t = 0; t < 60; t++) {
            sim_tick(&g_sim, &in);
            for (u32 i = 0; i < g_sim.cues.count; i++) {
                per_sound[g_sim.cues.items[i].id]++;
            }
            this_second += g_sim.cues.count;
            g_sim.cues.count = 0;
        }
        total += this_second;
        busiest = MAX(busiest, this_second);
    }
    fprintf(stderr, "  cues: %u in 60 s, busiest second %u (jump %u, land %u, crate_land %u)\n", total, busiest,
            per_sound[SFX_JUMP], per_sound[SFX_LAND], per_sound[SFX_CRATE_LAND]);
    CHECK(total > 30);          // something is heard
    CHECK(busiest <= 30);       // the gaps keep it readable
    CHECK(per_sound[SFX_JUMP] + per_sound[SFX_LAND] <= total / 2); // footwork does not drown the fight
    sim_free(&g_sim);
}

int main(void) {
    RUN_TEST(test_every_sound_is_synthesized_audible_and_ends_quietly);
    RUN_TEST(test_synthesis_is_deterministic_and_respects_the_buffer);
    RUN_TEST(test_events_make_the_matching_sounds);
    RUN_TEST(test_far_events_are_quieter_then_silent_and_panned);
    RUN_TEST(test_a_burst_makes_one_sound_and_the_gap_is_kept);
    RUN_TEST(test_the_end_of_a_match_sounds_like_its_result);
    RUN_TEST(test_only_the_players_base_sounds_the_alarm);
    RUN_TEST(test_an_elimination_sounds_unless_it_ends_the_match);
    RUN_TEST(test_ui_sounds_keep_their_gap);
    RUN_TEST(test_the_cue_list_merges_and_caps);
    RUN_TEST(test_menus_and_matches_queue_their_sounds);
    RUN_TEST(test_a_match_is_not_a_wall_of_noise);
    TEST_MAIN_END();
}
