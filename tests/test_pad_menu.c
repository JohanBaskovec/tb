// The pad construction menu: when it shows, where it is drawn, keyboard and mouse selection, and
// the whole thing driven through the real Sim (keys go through sim_tick, clicks through sim_click).
#include "test.h"

#include "app/sim.h"
#include "game/crate.h"
#include "game/tuning.h"
#include "test_helpers.h"

typedef struct Env {
    Sim sim;
    InputFrame in;
} Env;

// 24x12 map, floor rows 10-11, pad of team 1 at columns 8-11 (rows 8-9). The soldier spawns at column
// `spawn_col`. `pad_col` can move the pad, to test the right map edge.
static bool env_init(Env *e, int spawn_col, int pad_col) {
    TestMap m;
    map_init(&m, 24, 12);
    map_rect(&m, 0, 10, 23, 11, '#');
    map_put(&m, spawn_col, 9, '1');
    map_put(&m, pad_col, 9, 'P');
    char text[4096] = "";
    map_text(&m, text, sizeof(text));
    memset(e, 0, sizeof(*e));
    SimConfig cfg = {.level_text = text, .seed = 1};
    char err[256] = "";
    if (!sim_init(&e->sim, &cfg, err, sizeof(err))) {
        fprintf(stderr, "sim_init failed: %s\n", err);
        return false;
    }
    return true;
}

static void tick(Env *e, u32 mask) {
    input_frame_update(&e->in, mask);
    sim_tick(&e->sim, &e->in);
}

static void press(Env *e, u32 mask) {
    tick(e, mask);
    tick(e, 0);
}

static GameState *gs_of(Env *e) {
    return &e->sim.gs;
}

static bool rects_overlap(Rect a, Rect b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static void test_visibility(void) {
    Env e;
    CHECK(env_init(&e, 6, 8)); // soldier just left of the pad
    tick(&e, 0);
    CHECK(!e.sim.menu.visible); // empty pad: nothing to offer
    pad_debug_fill(gs_of(&e), 0, "....", "r...");
    tick(&e, 0);
    CHECK(e.sim.menu.visible);
    CHECK_EQ_U64(e.sim.menu.options.count, 1);
    sim_free(&e.sim);

    CHECK(env_init(&e, 0, 8)); // soldier far away (column 0, the pad starts at column 8)
    pad_debug_fill(gs_of(&e), 0, "....", "r...");
    tick(&e, 0);
    CHECK(!e.sim.menu.visible);
    // Walk towards the pad: the menu appears once close enough, and disappears again when leaving.
    bool seen = false;
    for (int i = 0; i < 120 && !seen; i++) {
        tick(&e, KEY(ACT_RIGHT));
        seen = e.sim.menu.visible;
    }
    CHECK(seen);
    for (int i = 0; i < 200 && e.sim.menu.visible; i++) {
        tick(&e, KEY(ACT_LEFT));
    }
    CHECK(!e.sim.menu.visible);
    sim_free(&e.sim);
}

static void test_layout_beside_the_pad(void) {
    Env e;
    CHECK(env_init(&e, 6, 8));
    pad_debug_fill(gs_of(&e), 0, "rrbr", "rrrb");
    tick(&e, 0);
    const PadMenu *m = &e.sim.menu;
    CHECK(m->visible && m->options.count >= 4);
    const Rect pad = pad_rect(gs_of(&e), 0);
    for (u32 i = 0; i < m->options.count; i++) {
        CHECK(!rects_overlap(m->rects[i], pad));        // beside the pad, not over it
        CHECK(m->rects[i].x >= pad.x + pad.w);          // on its right
        CHECK_NEAR(m->rects[i].w, PAD_MENU_ENTRY_W, 0.0);
        CHECK(m->rects[i].y >= 0.0f);
        for (u32 j = 0; j < i; j++) {
            CHECK(!rects_overlap(m->rects[i], m->rects[j]));
        }
    }
    // Entries read top to bottom in option order (biggest shape first).
    for (u32 i = 1; i < m->options.count && i < PAD_MENU_MAX_ROWS; i++) {
        CHECK(m->rects[i].y > m->rects[i - 1].y);
    }
    sim_free(&e.sim);

    // Pad against the right edge of the map: the menu goes to its left.
    CHECK(env_init(&e, 17, 20));
    pad_debug_fill(gs_of(&e), 0, "....", "rr.b");
    tick(&e, 0);
    m = &e.sim.menu;
    CHECK(m->visible);
    const Rect pad2 = pad_rect(gs_of(&e), 0);
    for (u32 i = 0; i < m->options.count; i++) {
        CHECK(m->rects[i].x + m->rects[i].w <= pad2.x);
        CHECK(m->rects[i].x >= 0.0f);
    }
    sim_free(&e.sim);
}

static void test_many_entries_wrap_in_columns(void) {
    Env e;
    CHECK(env_init(&e, 6, 8));
    pad_debug_fill(gs_of(&e), 0, "rrbr", "rrrb"); // many shapes, red and blue
    tick(&e, 0);
    const PadMenu *m = &e.sim.menu;
    CHECK(m->options.count > PAD_MENU_MAX_ROWS);
    CHECK(m->rects[PAD_MENU_MAX_ROWS].x > m->rects[0].x); // second column
    sim_free(&e.sim);
}

static void test_keyboard_select_and_confirm(void) {
    Env e;
    CHECK(env_init(&e, 6, 8));
    pad_debug_fill(gs_of(&e), 0, "r...", "rr.b");
    tick(&e, 0);
    PadMenu *m = &e.sim.menu;
    CHECK(m->visible);
    CHECK_EQ_U64(m->selected, 0);
    const u32 n = m->options.count;
    CHECK(n >= 3);
    press(&e, KEY(ACT_SWITCH));
    CHECK_EQ_U64(m->selected, 1);
    for (u32 i = 1; i < n; i++) {
        press(&e, KEY(ACT_SWITCH));
    }
    CHECK_EQ_U64(m->selected, 0); // wraps around

    // Select the second entry, remember what it is, validate: that exact thing is built.
    press(&e, KEY(ACT_SWITCH));
    const PadOption chosen = m->options.items[1];
    CHECK_EQ_U64(gs_of(&e)->products_built, 0);
    press(&e, KEY(ACT_CONFIRM));
    CHECK_EQ_U64(gs_of(&e)->products_built, 1);
    bool found = false;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *p = &gs_of(&e)->entities.slots[i];
        if (p->type == ENT_PRODUCT && p->as.product.kind == chosen.product) {
            found = true;
        }
    }
    CHECK(found);
    sim_free(&e.sim);
}

static void test_selection_sticks_to_the_entry(void) {
    Env e;
    CHECK(env_init(&e, 6, 8));
    pad_debug_fill(gs_of(&e), 0, "....", "rrb.");
    tick(&e, 0);
    PadMenu *m = &e.sim.menu;
    // Entries: 2x1 H red, 2x1 H blue, 1x1 red, 1x1 blue.
    CHECK_EQ_U64(m->options.count, 4);
    press(&e, KEY(ACT_SWITCH));
    press(&e, KEY(ACT_SWITCH));
    CHECK(m->options.items[m->selected].shape == SHAPE_1X1 && !m->options.items[m->selected].blue);
    // The options are recomputed every tick; an unchanged pad keeps the same selection.
    for (int i = 0; i < 30; i++) {
        tick(&e, 0);
    }
    CHECK(m->options.items[m->selected].shape == SHAPE_1X1 && !m->options.items[m->selected].blue);
    // A crate disappears from the pad (here: picked off by hand through the game API) and the entries
    // change: the selection follows the same (shape, color) if it still exists, else falls back to 0.
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        Entity *c = &gs_of(&e)->entities.slots[i];
        if (c->type == ENT_CRATE && c->as.crate.kind == CRATE_BLUE) {
            crate_destroy(gs_of(&e), (EntityId){i, c->generation});
        }
    }
    tick(&e, 0);
    CHECK_EQ_U64(m->options.count, 2); // 2x1 H red, 1x1 red
    CHECK(m->options.items[m->selected].shape == SHAPE_1X1 && !m->options.items[m->selected].blue);
    sim_free(&e.sim);
}

static void test_click_builds_the_entry_under_the_mouse(void) {
    Env e;
    CHECK(env_init(&e, 6, 8));
    pad_debug_fill(gs_of(&e), 0, "....", "rr..");
    tick(&e, 0);
    const PadMenu *m = &e.sim.menu;
    CHECK_EQ_U64(m->options.count, 2); // 2x1 H, 1x1
    CHECK(pad_menu_hit_test(m, vec2(0, 0)) == -1);
    CHECK(pad_menu_hit_test(m, vec2(m->rects[1].x + 1.0f, m->rects[1].y + 1.0f)) == 1);

    // Click on empty space, and just outside an entry: nothing.
    const Vec2 off = {m->rects[0].x - 3.0f, m->rects[0].y + 2.0f};
    CHECK(!sim_click(&e.sim, (Vec2){off.x - e.sim.cam.pos.x, off.y - e.sim.cam.pos.y}));
    CHECK_EQ_U64(gs_of(&e)->products_built, 0);

    // Click inside the second entry (1x1): converted from logical screen pixels like the real window does.
    const Rect r = m->rects[1];
    const Vec2 world = {r.x + r.w * 0.5f, r.y + r.h * 0.5f};
    const Vec2 logical = {world.x - roundf(e.sim.cam.pos.x), world.y - roundf(e.sim.cam.pos.y)};
    CHECK(sim_click(&e.sim, logical));
    CHECK_EQ_U64(gs_of(&e)->products_built, 1);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.consumed, 1); // a 1x1: one crate
    tick(&e, 0);
    CHECK_EQ_U64(crate_count(gs_of(&e)), 1);
    sim_free(&e.sim);
}

static void test_click_when_hidden_does_nothing(void) {
    Env e;
    CHECK(env_init(&e, 0, 8)); // far from the pad
    pad_debug_fill(gs_of(&e), 0, "....", "rr..");
    tick(&e, 0);
    CHECK(!e.sim.menu.visible);
    // Even right where the entries would be.
    for (int x = 0; x < 640; x += 8) {
        for (int y = 0; y < 360; y += 8) {
            CHECK(!sim_click(&e.sim, vec2((f32)x, (f32)y)));
        }
    }
    CHECK_EQ_U64(gs_of(&e)->products_built, 0);
    sim_free(&e.sim);
}

static void test_confirm_when_hidden_does_nothing(void) {
    Env e;
    CHECK(env_init(&e, 0, 8));
    pad_debug_fill(gs_of(&e), 0, "....", "rr..");
    for (int i = 0; i < 20; i++) {
        press(&e, KEY(ACT_CONFIRM));
        press(&e, KEY(ACT_SWITCH));
    }
    CHECK_EQ_U64(gs_of(&e)->products_built, 0);
    CHECK_EQ_U64(crate_count(gs_of(&e)), 2);
    sim_free(&e.sim);
}

static void test_building_everything_empties_the_pad(void) {
    Env e;
    CHECK(env_init(&e, 6, 8));
    pad_debug_fill(gs_of(&e), 0, "rr..", "rrrb");
    tick(&e, 0);
    u32 built = 0;
    for (int guard = 0; guard < 20 && e.sim.menu.visible; guard++) {
        press(&e, KEY(ACT_CONFIRM)); // always the first entry: the biggest shape
        built = gs_of(&e)->products_built;
    }
    CHECK(built >= 2);
    CHECK(!e.sim.menu.visible); // nothing left on the pad: the menu goes away
    CHECK_EQ_U64(crate_count(gs_of(&e)), 0);
    sim_free(&e.sim);
}

static void test_bot_with_a_pad_keeps_everything_consistent(void) {
    u32 total_built = 0;
    for (u64 seed = 1; seed <= 8; seed++) {
        Env e;
        CHECK(env_init(&e, 6, 8));
        e.sim.use_bot = true;
        bot_init(&e.sim.bot, seed);
        for (int t = 0; t < 4000; t++) {
            if (t % 300 == 0) {
                pad_debug_fill(gs_of(&e), 0, "rb.r", "rrbr"); // keep the pad stocked
            }
            sim_tick(&e.sim, NULL);
            const PadMenu *m = &e.sim.menu;
            if (m->visible) {
                CHECK(m->options.count > 0);
                CHECK(m->selected >= 0 && m->selected < (i32)m->options.count);
            }
        }
        total_built += gs_of(&e)->products_built;
        CHECK(gs_of(&e)->entities.live_count <= MAX_ENTITIES);
        sim_free(&e.sim);
    }
    CHECK(total_built > 0); // the bot mashes switch/confirm near the pad: something got built
}

int main(void) {
    RUN_TEST(test_visibility);
    RUN_TEST(test_layout_beside_the_pad);
    RUN_TEST(test_many_entries_wrap_in_columns);
    RUN_TEST(test_keyboard_select_and_confirm);
    RUN_TEST(test_selection_sticks_to_the_entry);
    RUN_TEST(test_click_builds_the_entry_under_the_mouse);
    RUN_TEST(test_click_when_hidden_does_nothing);
    RUN_TEST(test_confirm_when_hidden_does_nothing);
    RUN_TEST(test_building_everything_empties_the_pad);
    RUN_TEST(test_bot_with_a_pad_keeps_everything_consistent);
    TEST_MAIN_END();
}
