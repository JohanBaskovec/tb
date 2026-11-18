// The build goal: what the pad still needs (pure planning), the automatic build, the allies who fetch the right crates and
// stack them into the shape, and the middle-click ring that sets the goal.
#include "test.h"

#include "app/goal_menu.h"
#include "app/sim.h"
#include "game/combat.h"
#include "game/crate.h"
#include "game/goal.h"
#include "game/pad.h"
#include "game/squad.h"
#include "game/tuning.h"
#include "test_helpers.h"

#include <math.h>

#define FLOOR_Y 192.0f // feet of a soldier on the floor (floor rows 12-13 of the 14-row map)

static Sim g_sim;
static InputFrame g_in;

// 60x14 map: floor on rows 12-13, the human's spawn at column 1 and its pad at columns 8-11 (cells 4 x 2, bottom row 11).
static bool start(void) {
    TestMap m;
    map_init(&m, 60, 14);
    map_rect(&m, 0, 12, 59, 13, '#');
    map_put(&m, 1, 11, '1');
    map_put(&m, 8, 11, 'P');
    char text[8192];
    map_text(&m, text, sizeof(text));
    memset(&g_in, 0, sizeof(g_in));
    const SimConfig cfg = {.level_text = text, .seed = 1};
    char err[256] = "";
    if (!sim_init(&g_sim, &cfg, err, sizeof(err))) {
        fprintf(stderr, "sim_init failed: %s\n", err);
        return false;
    }
    return true;
}

static GameState *gs(void) {
    return &g_sim.gs;
}

static void tick_n(int n) {
    for (int i = 0; i < n; i++) {
        input_frame_update(&g_in, 0);
        sim_tick(&g_sim, &g_in);
    }
}

static i32 ally_at(f32 feet_x) {
    return soldier_spawn(gs(), PLAYER_TEAM, SOLDIER_NORMAL, vec2(feet_x, FLOOR_Y));
}

static EntityId free_crate(CrateKind kind, i32 cx) {
    return crate_spawn(gs(), kind, vec2((f32)(cx * TILE_SIZE), 11.0f * TILE_SIZE), CRATE_RESTING, -1);
}

static GoalPlan plan(void) {
    GoalPlan p;
    goal_plan(gs(), PLAYER_TEAM, 0, &p);
    return p;
}

static u32 count_product(PadProduct kind) {
    u32 n = 0;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        PadProduct p;
        n += entity_as_product(&gs()->entities.slots[i], &p) && p == kind ? 1u : 0u;
    }
    return n;
}

// Runs until something is built on the pad (an EV_PAD_BUILT): the product it made, or -1 if nothing was within `max` ticks.
// (A weapon made on the pad is picked up by the unarmed ally at once, so the map is no place to look for it.)
static i32 run_until_built(int max) {
    for (int i = 0; i < max; i++) {
        tick_n(1);
        for (u32 k = 0; k < gs()->events.count; k++) {
            if (gs()->events.items[k].type == EV_PAD_BUILT) {
                return gs()->events.items[k].a;
            }
        }
    }
    return -1;
}

// ---- Planning ------------------------------------------------------------------------------------

static void test_the_plan_says_what_the_pad_still_needs(void) {
    CHECK(start());
    CHECK(!plan().valid); // no goal, no plan
    goal_set(gs(), PLAYER_TEAM, SHAPE_2X1_H, false);
    GoalPlan p = plan();
    CHECK(p.valid && !p.buildable && p.feasible && p.missing == 2 && !p.needs_blue);
    CHECK(p.wanted[0] == 1 && p.wanted[1] == 1 && p.wanted[2] == 0 && p.wanted[3] == 0); // two bottom cells side by side

    goal_set(gs(), PLAYER_TEAM, SHAPE_L, false);
    p = plan();
    CHECK(p.missing == 3 && p.wanted[0] == 3 && p.wanted[1] == 1); // bottom pair, and the crate on top of the left one

    goal_set(gs(), PLAYER_TEAM, SHAPE_2X1_V, false);
    CHECK(pad_debug_fill(gs(), PLAYER_TEAM, "....", "r..."));
    p = plan();
    CHECK(p.missing == 1 && p.wanted[0] == 2 && p.filled[0] == 1); // it only needs the crate on top of the one there

    goal_set(gs(), PLAYER_TEAM, SHAPE_4X2, false);
    p = plan();
    CHECK(p.missing == 7 && p.feasible);
    sim_free(&g_sim);
}

static void test_a_shape_already_on_the_pad_is_buildable_in_the_right_colour(void) {
    CHECK(start());
    CHECK(pad_debug_fill(gs(), PLAYER_TEAM, "....", "rr.."));
    goal_set(gs(), PLAYER_TEAM, SHAPE_2X1_H, false);
    GoalPlan p = plan();
    CHECK(p.buildable && !p.build_blue && p.missing == 0);
    // A red goal accepts blue crates that are already there (and builds the blue version)...
    sim_free(&g_sim);
    CHECK(start());
    CHECK(pad_debug_fill(gs(), PLAYER_TEAM, "....", "bb.."));
    goal_set(gs(), PLAYER_TEAM, SHAPE_2X1_H, false);
    p = plan();
    CHECK(p.buildable && p.build_blue);
    // ... a blue goal needs one: two red crates are not enough, it wants a third one beside them.
    sim_free(&g_sim);
    CHECK(start());
    CHECK(pad_debug_fill(gs(), PLAYER_TEAM, "....", "rr.."));
    goal_set(gs(), PLAYER_TEAM, SHAPE_2X1_H, true);
    p = plan();
    CHECK(!p.buildable && p.feasible && p.needs_blue && p.missing == 1 && p.wanted[2] == 1);
    // One blue among them is.
    sim_free(&g_sim);
    CHECK(start());
    CHECK(pad_debug_fill(gs(), PLAYER_TEAM, "....", "rb.."));
    goal_set(gs(), PLAYER_TEAM, SHAPE_2X1_H, true);
    p = plan();
    CHECK(p.buildable && p.build_blue);
    sim_free(&g_sim);
}

static void test_a_goal_that_the_crates_in_the_way_forbid_is_not_feasible(void) {
    CHECK(start());
    CHECK(pad_debug_fill(gs(), PLAYER_TEAM, "rrrr", "rrrr")); // full of red: no room for a blue crate
    goal_set(gs(), PLAYER_TEAM, SHAPE_1X1, true);
    const GoalPlan p = plan();
    CHECK(p.valid && !p.buildable && !p.feasible);
    sim_free(&g_sim);
}

// ---- Building ------------------------------------------------------------------------------------

static void test_the_goal_is_built_once_the_order_is_collect_and_an_ally_is_there(void) {
    CHECK(start());
    CHECK(pad_debug_fill(gs(), PLAYER_TEAM, "....", "rr.."));
    goal_set(gs(), PLAYER_TEAM, SHAPE_2X1_H, false);
    gs()->teams[PLAYER_TEAM].order = ORDER_GUARD;
    ally_at(100.0f);
    tick_n(5);
    CHECK_EQ_U64(count_product(PRODUCT_GRENADES), 0); // not on this order
    gs()->teams[PLAYER_TEAM].order = ORDER_COLLECT;
    tick_n(2);
    CHECK_EQ_U64(count_product(PRODUCT_GRENADES), 1);
    CHECK(!gs()->teams[PLAYER_TEAM].goal.active); // done
    CHECK_EQ_U64(pad_crate_count(gs(), PLAYER_TEAM), 0);
    sim_free(&g_sim);

    // Alone, the human builds with the pad menu: nothing happens by itself.
    CHECK(start());
    CHECK(pad_debug_fill(gs(), PLAYER_TEAM, "....", "rr.."));
    goal_set(gs(), PLAYER_TEAM, SHAPE_2X1_H, false);
    gs()->teams[PLAYER_TEAM].order = ORDER_COLLECT;
    tick_n(30);
    CHECK_EQ_U64(count_product(PRODUCT_GRENADES), 0);
    CHECK(gs()->teams[PLAYER_TEAM].goal.active);
    sim_free(&g_sim);
}

// ---- The allies ----------------------------------------------------------------------------------

static void collect_goal(PadShape shape, bool blue) {
    goal_set(gs(), PLAYER_TEAM, shape, blue);
    gs()->teams[PLAYER_TEAM].order = ORDER_COLLECT;
}

static void test_allies_bring_only_the_crates_the_shape_needs(void) {
    CHECK(start());
    free_crate(CRATE_RED, 20);
    free_crate(CRATE_RED, 22);
    free_crate(CRATE_RED, 24);
    ally_at(300.0f);
    collect_goal(SHAPE_2X1_H, false);
    CHECK_EQ_U64(run_until_built(2400), PRODUCT_GRENADES);
    CHECK(!gs()->teams[PLAYER_TEAM].goal.active);
    tick_n(600);
    CHECK_EQ_U64(crate_count(gs()), 1); // the third one was left alone
    sim_free(&g_sim);
}

static void test_a_blue_goal_goes_for_the_blue_crate_even_when_it_is_farther(void) {
    CHECK(start());
    free_crate(CRATE_RED, 20);
    free_crate(CRATE_RED, 22);
    free_crate(CRATE_BLUE, 40);
    ally_at(300.0f);
    collect_goal(SHAPE_2X1_H, true);
    CHECK_EQ_U64(run_until_built(3000), PRODUCT_STRONG_GRENADES);
    sim_free(&g_sim);
}

static void test_a_red_goal_uses_a_blue_crate_already_on_the_pad(void) {
    CHECK(start());
    CHECK(pad_debug_fill(gs(), PLAYER_TEAM, "....", "b..."));
    free_crate(CRATE_RED, 20);
    free_crate(CRATE_BLUE, 22); // nearer to the pad's other side, but a red goal looks for red first
    ally_at(300.0f);
    collect_goal(SHAPE_2X1_H, false);
    CHECK_EQ_U64(run_until_built(2400), PRODUCT_STRONG_GRENADES); // the blue crate of the pad was used
    CHECK(!gs()->teams[PLAYER_TEAM].goal.active);
    u32 blue_left = 0;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs()->entities.slots[i];
        blue_left += e->type == ENT_CRATE && e->as.crate.kind == CRATE_BLUE ? 1u : 0u;
    }
    CHECK_EQ_U64(blue_left, 1); // the loose blue crate stayed where it was
    sim_free(&g_sim);
}

static void test_a_red_goal_falls_back_on_blue_crates_when_there_is_no_red_one(void) {
    CHECK(start());
    free_crate(CRATE_BLUE, 20);
    free_crate(CRATE_BLUE, 22);
    ally_at(300.0f);
    collect_goal(SHAPE_2X1_H, false);
    CHECK_EQ_U64(run_until_built(2400), PRODUCT_STRONG_GRENADES);
    sim_free(&g_sim);
}

static void test_a_blue_crate_already_on_its_way_counts(void) {
    CHECK(start());
    const i32 carrier = ally_at(120.0f);
    const EntityId carried = crate_spawn(gs(), CRATE_BLUE, vec2(0.0f, 0.0f), CRATE_CARRIED, -1);
    gs()->soldiers[carrier].c.carrying = carried;
    free_crate(CRATE_RED, 30);
    free_crate(CRATE_BLUE, 44); // would be fetched if the blue crate being carried were not counted
    ally_at(300.0f);
    collect_goal(SHAPE_2X1_H, true);
    CHECK_EQ_U64(run_until_built(3000), PRODUCT_STRONG_GRENADES);
    u32 blue_left = 0;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs()->entities.slots[i];
        blue_left += e->type == ENT_CRATE && e->as.crate.kind == CRATE_BLUE ? 1u : 0u;
    }
    CHECK_EQ_U64(blue_left, 1); // the loose blue one was not needed
    sim_free(&g_sim);
}

static void test_allies_stack_a_tall_shape_in_the_right_columns(void) {
    CHECK(start());
    for (int i = 0; i < 4; i++) {
        free_crate(CRATE_RED, 20 + 2 * i);
    }
    ally_at(300.0f);
    ally_at(320.0f);
    collect_goal(SHAPE_2X2, false);
    CHECK_EQ_U64(run_until_built(4500), PRODUCT_HEAVY_COMPANION);
    u32 heavy = 0;
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        heavy += gs()->soldiers[i].alive && gs()->soldiers[i].variant == SOLDIER_HEAVY ? 1u : 0u;
    }
    CHECK_EQ_U64(heavy, 1);
    CHECK(!gs()->teams[PLAYER_TEAM].goal.active);
    sim_free(&g_sim);
}

static void test_a_crate_already_on_the_pad_in_the_way_is_worked_around(void) {
    CHECK(start());
    CHECK(pad_debug_fill(gs(), PLAYER_TEAM, "....", ".r.."));
    free_crate(CRATE_RED, 30);
    free_crate(CRATE_RED, 33);
    ally_at(300.0f);
    collect_goal(SHAPE_2X1_H, false);
    CHECK_EQ_U64(run_until_built(3000), PRODUCT_GRENADES);
    sim_free(&g_sim);
}

// ---- The ring ------------------------------------------------------------------------------------

static Vec2 centre_of(Rect r) {
    return vec2(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
}

static void test_the_ring_fits_the_screen_and_every_entry_is_under_its_box(void) {
    GoalMenu menu = {0};
    CHECK(goal_menu_hit(&menu, vec2(100.0f, 100.0f)) == -1); // hidden
    const Vec2 spots[] = {{320.0f, 180.0f}, {0.0f, 0.0f}, {640.0f, 360.0f}, {10.0f, 350.0f}};
    for (usize s = 0; s < sizeof(spots) / sizeof(spots[0]); s++) {
        goal_menu_open(&menu, spots[s]);
        CHECK(menu.visible);
        u32 seen = 0;
        for (i32 i = 0; i < GOAL_MENU_ENTRIES; i++) {
            const Rect b = goal_menu_entry_box(&menu, i);
            CHECK(b.x >= 0.0f && b.y >= 0.0f && b.x + b.w <= (f32)LOGICAL_W && b.y + b.h <= (f32)LOGICAL_H);
            CHECK_EQ_U64(goal_menu_hit(&menu, centre_of(b)), i);
            seen |= 1u << i;
        }
        CHECK_EQ_U64(seen, (1u << GOAL_MENU_ENTRIES) - 1u);
        CHECK(goal_menu_hit(&menu, menu.center) == GOAL_MENU_CENTER);
        CHECK(goal_menu_hit(&menu, vec2(menu.center.x + 200.0f, menu.center.y)) == -1);
    }
    // Entry numbers map to a shape and a colour, and back.
    for (i32 i = 0; i < GOAL_MENU_ENTRIES; i++) {
        PadShape shape;
        bool blue;
        goal_menu_entry(i, &shape, &blue);
        CHECK_EQ_U64(goal_menu_index_of(shape, blue), i);
        PadCoord cells[PAD_CELLS];
        CHECK_EQ_U64(goal_shape_cells(shape, cells), pad_shape_size(shape)); // the picture has as many crates as the recipe
    }
    CHECK(!goal_menu_index_of(SHAPE_1X1, false) && goal_menu_index_of(SHAPE_1X1, true) == SHAPE_COUNT);
}

static void test_the_middle_click_sets_and_clears_the_goal(void) {
    CHECK(start());
    CHECK(!gs()->teams[PLAYER_TEAM].goal.active);
    sim_toggle_goal_menu(&g_sim, vec2(320.0f, 180.0f));
    CHECK(g_sim.goal_menu.visible);
    // Shoot nothing while it is open.
    combat_give_weapon(gs(), WEAPON_PISTOL);
    sim_set_mouse(&g_sim, vec2(320.0f, 60.0f), true, false);
    tick_n(5);
    CHECK_EQ_U64(gs()->combat_stats.shots, 0);
    sim_set_mouse(&g_sim, vec2(320.0f, 60.0f), false, false);
    // A click on the entry of a blue 2x2 sets the goal and closes the ring.
    const i32 entry = goal_menu_index_of(SHAPE_2X2, true);
    CHECK(sim_click(&g_sim, centre_of(goal_menu_entry_box(&g_sim.goal_menu, entry))));
    CHECK(!g_sim.goal_menu.visible);
    CHECK(gs()->teams[PLAYER_TEAM].goal.active && gs()->teams[PLAYER_TEAM].goal.shape == SHAPE_2X2 &&
          gs()->teams[PLAYER_TEAM].goal.blue);
    // The middle disc clears it.
    sim_toggle_goal_menu(&g_sim, vec2(320.0f, 180.0f));
    CHECK(sim_click(&g_sim, g_sim.goal_menu.center));
    CHECK(!gs()->teams[PLAYER_TEAM].goal.active && !g_sim.goal_menu.visible);
    // Elsewhere, or a second middle click, only closes it.
    goal_set(gs(), PLAYER_TEAM, SHAPE_1X1, false);
    sim_toggle_goal_menu(&g_sim, vec2(320.0f, 180.0f));
    CHECK(!sim_click(&g_sim, vec2(2.0f, 2.0f)));
    CHECK(!g_sim.goal_menu.visible && gs()->teams[PLAYER_TEAM].goal.active);
    sim_toggle_goal_menu(&g_sim, vec2(320.0f, 180.0f));
    sim_toggle_goal_menu(&g_sim, vec2(320.0f, 180.0f));
    CHECK(!g_sim.goal_menu.visible);
    // A right click cancels it too.
    sim_toggle_goal_menu(&g_sim, vec2(320.0f, 180.0f));
    sim_set_mouse(&g_sim, vec2(300.0f, 150.0f), false, true);
    CHECK(!g_sim.goal_menu.visible);
    sim_free(&g_sim);
}

static void test_the_ring_needs_a_human_team_with_a_pad(void) {
    CHECK(start());
    gs()->controlled = -1; // spectating
    sim_toggle_goal_menu(&g_sim, vec2(320.0f, 180.0f));
    CHECK(!g_sim.goal_menu.visible);
    sim_free(&g_sim);
}

int main(void) {
    RUN_TEST(test_the_plan_says_what_the_pad_still_needs);
    RUN_TEST(test_a_shape_already_on_the_pad_is_buildable_in_the_right_colour);
    RUN_TEST(test_a_goal_that_the_crates_in_the_way_forbid_is_not_feasible);
    RUN_TEST(test_the_goal_is_built_once_the_order_is_collect_and_an_ally_is_there);
    RUN_TEST(test_allies_bring_only_the_crates_the_shape_needs);
    RUN_TEST(test_a_blue_goal_goes_for_the_blue_crate_even_when_it_is_farther);
    RUN_TEST(test_a_red_goal_uses_a_blue_crate_already_on_the_pad);
    RUN_TEST(test_a_red_goal_falls_back_on_blue_crates_when_there_is_no_red_one);
    RUN_TEST(test_a_blue_crate_already_on_its_way_counts);
    RUN_TEST(test_allies_stack_a_tall_shape_in_the_right_columns);
    RUN_TEST(test_a_crate_already_on_the_pad_in_the_way_is_worked_around);
    RUN_TEST(test_the_ring_fits_the_screen_and_every_entry_is_under_its_box);
    RUN_TEST(test_the_middle_click_sets_and_clears_the_goal);
    RUN_TEST(test_the_ring_needs_a_human_team_with_a_pad);
    TEST_MAIN_END();
}
