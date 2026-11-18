// The screens around a match: title -> setup -> match -> pause / results -> title, with keys and mouse.
#include "test.h"

#include "app/front.h"
#include "game/tuning.h"
#include "test_helpers.h"

static Front g_front; // a whole match inside: not on the stack
static InputFrame g_in;

static void init(void) {
    const FrontConfig cfg = {.assets_dir = CS_ASSET_DIR, .seed = 7};
    CHECK(front_init(&g_front, &cfg));
    g_in = (InputFrame){0};
}

static void idle(int ticks) {
    for (int i = 0; i < ticks; i++) {
        input_frame_update(&g_in, 0);
        front_tick(&g_front, &g_in);
    }
}

// One press: a tick with the key down, then a tick with it released.
static void press(Action a) {
    input_frame_update(&g_in, KEY(a));
    front_tick(&g_front, &g_in);
    idle(1);
}

static Vec2 center_of(i32 entry) {
    const Rect r = front_menu_rect(&g_front, entry);
    return vec2(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
}

// From the title screen to a match on the selected level.
static void start_match(void) {
    press(ACT_CONFIRM); // title: play
    CHECK(g_front.screen == SCREEN_SETUP && g_front.selected == SETUP_START);
    press(ACT_CONFIRM);
    CHECK(g_front.screen == SCREEN_PLAY);
}

static void test_every_offered_level_is_a_two_team_level(void) {
    for (i32 i = 0; i < FRONT_LEVEL_COUNT; i++) {
        Arena arena;
        Level lv;
        char path[512], err[128];
        snprintf(path, sizeof(path), "%s/levels/%s", CS_ASSET_DIR, FRONT_LEVELS[i].file);
        CHECK(arena_init(&arena, 1u << 20));
        CHECK(level_load_file(&lv, &arena, path, err, sizeof(err)));
        CHECK(lv.has_spawn[0] && lv.pads[0].present && lv.has_spawn[1] && lv.pads[1].present);
        arena_free(&arena);
    }
    CHECK(strcmp(FRONT_LEVELS[0].file, "beach.lvl") == 0); // the first real level comes first
}

static void test_the_title_screen_plays_or_quits(void) {
    init();
    CHECK(g_front.screen == SCREEN_TITLE && g_front.selected == TITLE_PLAY);
    CHECK(front_sim(&g_front) == NULL);
    press(ACT_DOWN);
    CHECK(g_front.selected == TITLE_QUIT);
    press(ACT_DOWN); // wraps around
    CHECK(g_front.selected == TITLE_PLAY);
    press(ACT_UP);
    press(ACT_JUMP); // Space validates too
    CHECK(g_front.screen == SCREEN_QUIT);
    idle(5); // nothing happens any more
    CHECK(g_front.screen == SCREEN_QUIT);
    front_free(&g_front);
}

static void test_the_setup_screen_changes_level_and_mode(void) {
    init();
    press(ACT_CONFIRM);
    CHECK(g_front.screen == SCREEN_SETUP);
    CHECK(g_front.preview_ok && strcmp(g_front.preview.name, "La Crique") == 0);
    press(ACT_UP);
    press(ACT_UP);
    press(ACT_UP);
    CHECK(g_front.selected == SETUP_LEVEL);
    press(ACT_RIGHT);
    CHECK(g_front.level_index == 1 && g_front.preview_ok && strcmp(g_front.preview.name, "AI Arena") == 0);
    press(ACT_LEFT);
    press(ACT_LEFT); // wraps around
    CHECK(g_front.level_index == FRONT_LEVEL_COUNT - 1);
    press(ACT_DOWN);
    CHECK(g_front.selected == SETUP_MODE && !g_front.spectate);
    press(ACT_RIGHT);
    CHECK(g_front.spectate);
    press(ACT_CONFIRM); // confirm on a setting changes it too
    CHECK(!g_front.spectate);
    press(ACT_DOWN);
    CHECK(g_front.selected == SETUP_SQUAD && g_front.squad == 1); // one soldier each by default
    press(ACT_RIGHT);
    press(ACT_RIGHT);
    CHECK(g_front.squad == 3);
    press(ACT_LEFT);
    press(ACT_LEFT);
    press(ACT_LEFT); // wraps around
    CHECK(g_front.squad == TUNING_SKIRMISH_MAX_SQUAD);
    press(ACT_PAUSE); // back
    CHECK(g_front.screen == SCREEN_TITLE);
    front_free(&g_front);
}

static void test_a_match_starts_against_the_ai(void) {
    init();
    start_match();
    Sim *sim = front_sim(&g_front);
    CHECK(sim != NULL);
    CHECK(strcmp(sim->level.name, "La Crique") == 0);
    CHECK(!sim->gs.teams[0].ai && sim->gs.teams[1].ai); // the human against an AI team
    CHECK(sim->gs.controlled >= 0);
    CHECK(team_soldier_count(&sim->gs, 0) == TUNING_SKIRMISH_START_SQUAD); // a skirmish starts with a small squad
    CHECK(team_soldier_count(&sim->gs, 1) == TUNING_SKIRMISH_START_SQUAD);
    CHECK(sim->seed == 7 && g_front.seed == 8); // the next match takes the next seed
    // The keys go to the soldier.
    const f32 x0 = game_player(&sim->gs)->body.pos.x;
    for (int i = 0; i < 30; i++) {
        input_frame_update(&g_in, KEY(ACT_RIGHT));
        front_tick(&g_front, &g_in);
    }
    CHECK(game_player(&sim->gs)->body.pos.x > x0 + 20.0f);
    CHECK(sim->gs.tick == 31);
    front_free(&g_front);
}

static void test_the_chosen_squad_size_is_used(void) {
    init();
    press(ACT_CONFIRM);
    press(ACT_UP); // squad
    press(ACT_RIGHT);
    press(ACT_RIGHT);
    press(ACT_DOWN); // start
    press(ACT_CONFIRM);
    Sim *sim = front_sim(&g_front);
    CHECK(sim != NULL);
    CHECK(team_soldier_count(&sim->gs, 0) == 3 && team_soldier_count(&sim->gs, 1) == 3);
    front_free(&g_front);
}

static void test_a_spectated_match_has_no_human(void) {
    init();
    press(ACT_CONFIRM);
    press(ACT_UP);
    press(ACT_UP);
    press(ACT_RIGHT); // mode: AI against AI
    press(ACT_DOWN);
    press(ACT_DOWN);
    press(ACT_CONFIRM);
    CHECK(g_front.screen == SCREEN_PLAY);
    const Sim *sim = front_sim(&g_front);
    CHECK(sim->gs.teams[0].ai && sim->gs.teams[1].ai && sim->gs.controlled < 0);
    idle(600);
    CHECK(team_soldier_count(&sim->gs, 0) >= 1 && team_soldier_count(&sim->gs, 1) >= 1);
    front_free(&g_front);
}

static void test_pause_freezes_resumes_restarts_and_quits(void) {
    init();
    start_match();
    Sim *sim = front_sim(&g_front);
    idle(20);
    const u64 tick = sim->gs.tick;
    press(ACT_PAUSE);
    CHECK(g_front.screen == SCREEN_PAUSE && g_front.selected == PAUSE_RESUME);
    idle(30);
    CHECK(sim->gs.tick == tick); // frozen
    press(ACT_PAUSE); // Escape again: back to the match (the release tick already plays)
    CHECK(g_front.screen == SCREEN_PLAY);
    idle(10);
    CHECK(sim->gs.tick == tick + 11);
    press(ACT_PAUSE);
    press(ACT_DOWN);
    press(ACT_CONFIRM); // restart
    CHECK(g_front.screen == SCREEN_PLAY && sim->gs.tick == 1);
    press(ACT_PAUSE);
    press(ACT_UP); // wraps to "give up"
    CHECK(g_front.selected == PAUSE_QUIT);
    press(ACT_CONFIRM);
    CHECK(g_front.screen == SCREEN_TITLE && front_sim(&g_front) == NULL && !g_front.sim_ready);
    front_free(&g_front);
}

static void kill_team(Sim *sim, i32 team) {
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        if (sim->gs.soldiers[i].alive && sim->gs.soldiers[i].team == team) {
            sim->gs.soldiers[i].c.hp = 0;
        }
    }
}

static void test_the_results_come_after_the_match_and_lead_to_a_new_one(void) {
    init();
    start_match();
    Sim *sim = front_sim(&g_front);
    idle(5);
    kill_team(sim, 1);
    idle(2);
    CHECK(sim->gs.match.over && sim->gs.match.winner == 0);
    CHECK(g_front.screen == SCREEN_PLAY); // the match goes on for a moment...
    idle((int)SECONDS_TO_TICKS(TUNING_RESULTS_DELAY_S));
    CHECK(g_front.screen == SCREEN_RESULTS); // ... then the results
    const u64 frozen = sim->gs.tick;
    idle(30);
    CHECK(sim->gs.tick == frozen);
    CHECK(front_sim(&g_front) != NULL); // still there to be drawn behind the results
    press(ACT_RIGHT);
    CHECK(g_front.selected == RESULTS_MENU);
    press(ACT_LEFT);
    press(ACT_CONFIRM); // play again: a new match, the next seed
    CHECK(g_front.screen == SCREEN_PLAY && sim->gs.tick == 1 && !sim->gs.match.over && sim->seed == 8);

    kill_team(sim, 0); // this time the human loses
    idle((int)SECONDS_TO_TICKS(TUNING_RESULTS_DELAY_S) + 2);
    CHECK(g_front.screen == SCREEN_RESULTS && sim->gs.match.winner == 1);
    press(ACT_DOWN);
    press(ACT_CONFIRM); // main menu
    CHECK(g_front.screen == SCREEN_TITLE && !g_front.sim_ready);
    front_free(&g_front);
}

static void test_the_mouse_points_and_clicks_entries(void) {
    init();
    front_point(&g_front, center_of(TITLE_QUIT));
    CHECK(g_front.selected == TITLE_QUIT);
    front_point(&g_front, vec2(2.0f, 2.0f)); // nowhere: the selection stays
    CHECK(g_front.selected == TITLE_QUIT);
    CHECK(!front_click(&g_front, vec2(2.0f, 2.0f)));
    CHECK(front_click(&g_front, center_of(TITLE_PLAY)));
    CHECK(g_front.screen == SCREEN_SETUP);
    CHECK(front_click(&g_front, center_of(SETUP_LEVEL))); // a setting: changed
    CHECK(g_front.level_index == 1);
    CHECK(front_click(&g_front, center_of(SETUP_START)));
    CHECK(g_front.screen == SCREEN_PLAY);
    CHECK(strcmp(front_sim(&g_front)->level.name, "AI Arena") == 0);
    CHECK(!front_click(&g_front, vec2(320.0f, 180.0f))); // in a match, a click belongs to the game
    front_free(&g_front);
}

static void test_a_missing_level_keeps_the_setup_screen_with_an_error(void) {
    const FrontConfig cfg = {.assets_dir = "/nonexistent", .seed = 1};
    CHECK(front_init(&g_front, &cfg));
    g_in = (InputFrame){0};
    press(ACT_CONFIRM);
    CHECK(!g_front.preview_ok);
    press(ACT_CONFIRM);
    CHECK(g_front.screen == SCREEN_SETUP && g_front.error[0] != '\0' && front_sim(&g_front) == NULL);
    front_free(&g_front);
}

int main(void) {
    RUN_TEST(test_every_offered_level_is_a_two_team_level);
    RUN_TEST(test_the_title_screen_plays_or_quits);
    RUN_TEST(test_the_setup_screen_changes_level_and_mode);
    RUN_TEST(test_a_match_starts_against_the_ai);
    RUN_TEST(test_the_chosen_squad_size_is_used);
    RUN_TEST(test_a_spectated_match_has_no_human);
    RUN_TEST(test_pause_freezes_resumes_restarts_and_quits);
    RUN_TEST(test_the_results_come_after_the_match_and_lead_to_a_new_one);
    RUN_TEST(test_the_mouse_points_and_clicks_entries);
    RUN_TEST(test_a_missing_level_keeps_the_setup_screen_with_an_error);
    TEST_MAIN_END();
}
