// The stacking pad: pure recipe rules (every shape, red/blue, consumption order, exhaustive
// invariants over all 3^8 grids) and the world side (reading the crates, pad_build, products).
#include "test.h"

#include "app/sim.h"
#include "game/crate.h"
#include "game/pad.h"
#include "test_helpers.h"

// ---- Pure rules --------------------------------------------------------------------------------

// Rows are given top first, 4 characters each: '.' empty, 'r' red, 'b' blue.
static PadGrid grid(const char *top, const char *bottom) {
    PadGrid g = {0};
    const char *rows[PAD_ROWS] = {top, bottom};
    for (int r = 0; r < PAD_ROWS; r++) {
        for (int c = 0; c < PAD_COLS; c++) {
            g.cells[r][c] = rows[r][c] == 'r' ? PAD_RED : rows[r][c] == 'b' ? PAD_BLUE : PAD_EMPTY;
        }
    }
    return g;
}

static const PadOption *find(const PadOptionList *l, PadShape shape, bool blue) {
    for (u32 i = 0; i < l->count; i++) {
        if (l->items[i].shape == shape && l->items[i].blue == blue) {
            return &l->items[i];
        }
    }
    return NULL;
}

static bool option_has_cell(const PadOption *o, int col, int row) {
    for (u32 i = 0; i < o->count; i++) {
        if (o->cells[i].col == col && o->cells[i].row == row) {
            return true;
        }
    }
    return false;
}

static void test_empty_pad_offers_nothing(void) {
    PadOptionList l;
    PadGrid g = grid("....", "....");
    pad_options(&g, &l);
    CHECK_EQ_U64(l.count, 0);
}

static void test_recipe_table(void) {
    CHECK(pad_recipe(SHAPE_1X1, false) == PRODUCT_PISTOL);
    CHECK(pad_recipe(SHAPE_1X1, true) == PRODUCT_UZI);
    CHECK(pad_recipe(SHAPE_2X1_H, false) == PRODUCT_GRENADES);
    CHECK(pad_recipe(SHAPE_2X1_H, true) == PRODUCT_STRONG_GRENADES);
    CHECK(pad_recipe(SHAPE_2X1_V, false) == PRODUCT_COMPANION);
    CHECK(pad_recipe(SHAPE_2X1_V, true) == PRODUCT_STRONG_COMPANION);
    CHECK(pad_recipe(SHAPE_L, false) == PRODUCT_UZI);
    CHECK(pad_recipe(SHAPE_L, true) == PRODUCT_ROCKET_LAUNCHER);
    CHECK(pad_recipe(SHAPE_2X2, false) == PRODUCT_HEAVY_COMPANION);
    CHECK(pad_recipe(SHAPE_2X2, true) == PRODUCT_HEAVY_ARMORED_COMPANION);
    CHECK(pad_recipe(SHAPE_4X2, false) == PRODUCT_TANK);
    CHECK(pad_recipe(SHAPE_4X2, true) == PRODUCT_HEAVY_TANK);
    CHECK_EQ_U64(pad_shape_size(SHAPE_1X1), 1);
    CHECK_EQ_U64(pad_shape_size(SHAPE_2X1_H), 2);
    CHECK_EQ_U64(pad_shape_size(SHAPE_2X1_V), 2);
    CHECK_EQ_U64(pad_shape_size(SHAPE_L), 3);
    CHECK_EQ_U64(pad_shape_size(SHAPE_2X2), 4);
    CHECK_EQ_U64(pad_shape_size(SHAPE_4X2), 8);
    for (int p = 0; p < PRODUCT_COUNT; p++) {
        CHECK(pad_product_name((PadProduct)p)[0] != '?');
    }
}

static void test_single_crate_everywhere(void) {
    for (int r = 0; r < PAD_ROWS; r++) {
        for (int c = 0; c < PAD_COLS; c++) {
            for (int blue = 0; blue < 2; blue++) {
                PadGrid g = {0};
                g.cells[r][c] = blue ? PAD_BLUE : PAD_RED;
                PadOptionList l;
                pad_options(&g, &l);
                CHECK_EQ_U64(l.count, 1);
                CHECK(l.items[0].shape == SHAPE_1X1);
                CHECK(l.items[0].blue == (blue == 1));
                CHECK(l.items[0].product == (blue ? PRODUCT_UZI : PRODUCT_PISTOL));
                CHECK(option_has_cell(&l.items[0], c, r));
            }
        }
    }
}

static void test_horizontal_pair(void) {
    PadOptionList l;
    PadGrid g = grid("....", ".rr.");
    pad_options(&g, &l);
    CHECK(find(&l, SHAPE_2X1_H, false) != NULL);
    CHECK(find(&l, SHAPE_2X1_H, true) == NULL);
    CHECK(find(&l, SHAPE_2X1_V, false) == NULL);
    CHECK(find(&l, SHAPE_1X1, false) != NULL);

    g = grid("....", ".rb.");
    pad_options(&g, &l);
    CHECK(find(&l, SHAPE_2X1_H, true) != NULL); // one blue crate in the shape upgrades it
    CHECK(find(&l, SHAPE_2X1_H, false) == NULL);
    CHECK(find(&l, SHAPE_2X1_H, true)->product == PRODUCT_STRONG_GRENADES);

    g = grid("....", "r.r."); // not adjacent: not a pair
    pad_options(&g, &l);
    CHECK(find(&l, SHAPE_2X1_H, false) == NULL);
}

static void test_vertical_pair(void) {
    PadOptionList l;
    PadGrid g = grid("..r.", "..r.");
    pad_options(&g, &l);
    const PadOption *o = find(&l, SHAPE_2X1_V, false);
    CHECK(o != NULL && o->product == PRODUCT_COMPANION);
    CHECK(find(&l, SHAPE_2X1_H, false) == NULL);

    g = grid("..b.", "..r.");
    pad_options(&g, &l);
    o = find(&l, SHAPE_2X1_V, true);
    CHECK(o != NULL && o->product == PRODUCT_STRONG_COMPANION);
    CHECK(find(&l, SHAPE_2X1_V, false) == NULL);
}

static void test_l_both_orientations(void) {
    PadOptionList l;
    PadGrid left = grid("r...", "rr..");  // top crate on the left end
    PadGrid right = grid(".r..", ".rr."); // top crate on the right end
    PadGrid g = left;
    pad_options(&g, &l);
    const PadOption *o = find(&l, SHAPE_L, false);
    CHECK(o != NULL && o->product == PRODUCT_UZI && o->count == 3);
    g = right;
    pad_options(&g, &l);
    o = find(&l, SHAPE_L, false);
    CHECK(o != NULL && o->count == 3);

    g = grid("b...", "rr..");
    pad_options(&g, &l);
    o = find(&l, SHAPE_L, true);
    CHECK(o != NULL && o->product == PRODUCT_ROCKET_LAUNCHER);

    g = grid("..r.", "..rr"); // L at the far right
    pad_options(&g, &l);
    CHECK(find(&l, SHAPE_L, false) != NULL);

    // A top crate over one crate of a wider bottom row is still an L (it sits on one end of a pair).
    g = grid(".r..", "rrr.");
    pad_options(&g, &l);
    CHECK(find(&l, SHAPE_L, false) != NULL);
    // A crate on top of nothing is not an L.
    g = grid("r...", ".rr.");
    pad_options(&g, &l);
    CHECK(find(&l, SHAPE_L, false) == NULL);
}

static void test_two_by_two(void) {
    PadOptionList l;
    PadGrid g = grid(".rr.", ".rr.");
    pad_options(&g, &l);
    const PadOption *o = find(&l, SHAPE_2X2, false);
    CHECK(o != NULL && o->product == PRODUCT_HEAVY_COMPANION && o->count == 4);
    CHECK(find(&l, SHAPE_L, false) != NULL); // contained shapes are offered too
    CHECK(find(&l, SHAPE_2X1_V, false) != NULL);

    g = grid(".rb.", ".rr.");
    pad_options(&g, &l);
    o = find(&l, SHAPE_2X2, true);
    CHECK(o != NULL && o->product == PRODUCT_HEAVY_ARMORED_COMPANION);
    CHECK(find(&l, SHAPE_2X2, false) == NULL);
}

static void test_full_pad_is_a_tank(void) {
    PadOptionList l;
    PadGrid g = grid("rrrr", "rrrr");
    pad_options(&g, &l);
    const PadOption *o = find(&l, SHAPE_4X2, false);
    CHECK(o != NULL && o->product == PRODUCT_TANK && o->count == 8);
    CHECK(l.items[0].shape == SHAPE_4X2); // biggest first
    g = grid("rrrr", "rrbr");
    pad_options(&g, &l);
    o = find(&l, SHAPE_4X2, true);
    CHECK(o != NULL && o->product == PRODUCT_HEAVY_TANK);
    CHECK(find(&l, SHAPE_4X2, false) == NULL);
    // ...but the all-red sub-shapes remain available.
    CHECK(find(&l, SHAPE_2X2, false) != NULL);
}

static void test_blue_choice_when_both_exist(void) {
    // Two red and one blue crate in a row: the pair can be made red (the two reds) or blue.
    PadOptionList l;
    PadGrid g = grid("....", "rrb.");
    pad_options(&g, &l);
    CHECK(find(&l, SHAPE_2X1_H, false) != NULL);
    CHECK(find(&l, SHAPE_2X1_H, true) != NULL);
    const PadOption *red = find(&l, SHAPE_2X1_H, false);
    CHECK(option_has_cell(red, 0, 1) && option_has_cell(red, 1, 1) && !option_has_cell(red, 2, 1));
    CHECK(find(&l, SHAPE_1X1, false) != NULL);
    CHECK(find(&l, SHAPE_1X1, true) != NULL);
}

static void test_consumption_starts_from_the_top(void) {
    PadOptionList l;
    PadGrid g = grid(".r..", ".r..");
    pad_options(&g, &l);
    const PadOption *o = find(&l, SHAPE_1X1, false);
    CHECK(o != NULL && o->count == 1);
    CHECK(o->cells[0].row == 0 && o->cells[0].col == 1); // the top crate, not the bottom one

    // Cells of any option are listed top row first, then left to right.
    g = grid("rrrr", "rrrr");
    pad_options(&g, &l);
    o = find(&l, SHAPE_4X2, false);
    for (u32 i = 1; i < o->count; i++) {
        const PadCoord a = o->cells[i - 1], b = o->cells[i];
        CHECK(a.row < b.row || (a.row == b.row && a.col < b.col));
    }

    // Among equivalent pairs the highest one goes first: a pair on the top row is consumed before
    // the same pair on the bottom row.
    g = grid("rr..", "rr..");
    pad_options(&g, &l);
    o = find(&l, SHAPE_2X1_H, false);
    CHECK(option_has_cell(o, 0, 0) && option_has_cell(o, 1, 0));

    // Leftmost wins on a tie.
    g = grid("....", "r..r");
    pad_options(&g, &l);
    o = find(&l, SHAPE_1X1, false);
    CHECK(option_has_cell(o, 0, 1));
}

static void test_options_are_ordered_biggest_first_red_before_blue(void) {
    PadOptionList l;
    PadGrid g = grid("rrbr", "rrrb");
    pad_options(&g, &l);
    for (u32 i = 1; i < l.count; i++) {
        const PadOption *a = &l.items[i - 1], *b = &l.items[i];
        CHECK(a->shape > b->shape || (a->shape == b->shape && !a->blue && b->blue));
    }
}

// Independent presence check for the exhaustive test: written shape by shape, differently from the
// implementation. Sets what colors a shape can be made in.
static void expected_presence(const PadGrid *g, PadShape shape, bool *red, bool *blue) {
    *red = *blue = false;
#define F(c, r) (g->cells[r][c] != PAD_EMPTY)
#define B(c, r) (g->cells[r][c] == PAD_BLUE)
    // Calls `note` for a placement given as a list of cells.
    struct {
        bool full, any_blue;
    } p;
#define BEGIN p.full = true; p.any_blue = false
#define ADD(c, r) do { p.full = p.full && F(c, r); p.any_blue = p.any_blue || (F(c, r) && B(c, r)); } while (0)
#define END do { if (p.full) { if (p.any_blue) *blue = true; else *red = true; } } while (0)
    switch (shape) {
    case SHAPE_1X1:
        for (int r = 0; r < 2; r++) for (int c = 0; c < 4; c++) { BEGIN; ADD(c, r); END; }
        break;
    case SHAPE_2X1_H:
        for (int r = 0; r < 2; r++) for (int c = 0; c < 3; c++) { BEGIN; ADD(c, r); ADD(c + 1, r); END; }
        break;
    case SHAPE_2X1_V:
        for (int c = 0; c < 4; c++) { BEGIN; ADD(c, 0); ADD(c, 1); END; }
        break;
    case SHAPE_L:
        for (int c = 0; c < 3; c++) {
            BEGIN; ADD(c, 1); ADD(c + 1, 1); ADD(c, 0); END;
            BEGIN; ADD(c, 1); ADD(c + 1, 1); ADD(c + 1, 0); END;
        }
        break;
    case SHAPE_2X2:
        for (int c = 0; c < 3; c++) { BEGIN; ADD(c, 0); ADD(c + 1, 0); ADD(c, 1); ADD(c + 1, 1); END; }
        break;
    case SHAPE_4X2:
        BEGIN;
        for (int c = 0; c < 4; c++) { ADD(c, 0); ADD(c, 1); }
        END;
        break;
    default:
        break;
    }
#undef F
#undef B
#undef BEGIN
#undef ADD
#undef END
}

static void test_exhaustive_all_grids(void) {
    u32 grids = 0, nonempty = 0;
    for (u32 code = 0; code < 6561; code++) { // 3^8
        PadGrid g = {0};
        u32 x = code;
        u32 crates = 0;
        for (int r = 0; r < PAD_ROWS; r++) {
            for (int c = 0; c < PAD_COLS; c++) {
                g.cells[r][c] = (u8)(x % 3);
                crates += x % 3 != 0 ? 1u : 0u;
                x /= 3;
            }
        }
        PadOptionList l;
        pad_options(&g, &l);
        grids++;
        if (crates == 0) {
            CHECK_EQ_U64(l.count, 0);
            continue;
        }
        nonempty++;
        // A non-empty pad always offers at least a 1x1.
        CHECK(find(&l, SHAPE_1X1, false) != NULL || find(&l, SHAPE_1X1, true) != NULL);

        // Same set of options as the independent computation, with no duplicates.
        u32 expected_count = 0;
        for (int s = 0; s < SHAPE_COUNT; s++) {
            bool red, blue;
            expected_presence(&g, (PadShape)s, &red, &blue);
            CHECK((find(&l, (PadShape)s, false) != NULL) == red);
            CHECK((find(&l, (PadShape)s, true) != NULL) == blue);
            expected_count += (red ? 1u : 0u) + (blue ? 1u : 0u);
        }
        CHECK_EQ_U64(l.count, expected_count);

        for (u32 i = 0; i < l.count; i++) {
            const PadOption *o = &l.items[i];
            CHECK_EQ_U64(o->count, pad_shape_size(o->shape));
            CHECK(o->product == pad_recipe(o->shape, o->blue));
            bool any_blue = false;
            for (u32 k = 0; k < o->count; k++) {
                const u8 cell = g.cells[o->cells[k].row][o->cells[k].col];
                CHECK(cell != PAD_EMPTY); // only existing crates are consumed
                any_blue = any_blue || cell == PAD_BLUE;
                if (k > 0) {
                    const PadCoord a = o->cells[k - 1], b = o->cells[k];
                    CHECK(a.row < b.row || (a.row == b.row && a.col < b.col));
                }
                for (u32 j = 0; j < k; j++) { // no cell twice
                    CHECK(o->cells[j].row != o->cells[k].row || o->cells[j].col != o->cells[k].col);
                }
            }
            CHECK(any_blue == o->blue);
            if (i > 0) {
                const PadOption *a = &l.items[i - 1];
                CHECK(a->shape > o->shape || (a->shape == o->shape && !a->blue && o->blue));
            }
        }
    }
    CHECK_EQ_U64(grids, 6561);
    CHECK_EQ_U64(nonempty, 6560);
}

// ---- World -------------------------------------------------------------------------------------

typedef struct Env {
    Sim sim;
    InputFrame in;
} Env;

// 20x12 map, floor rows 10-11. Pad of team 1 at columns 6-9, rows 8-9 (marker on the bottom-left cell).
static bool env_init(Env *e, const char *header, bool with_pad) {
    TestMap m;
    map_init(&m, 20, 12);
    map_rect(&m, 0, 10, 19, 11, '#');
    map_put(&m, 1, 9, '1');
    if (with_pad) {
        map_put(&m, 6, 9, 'P');
    }
    char text[8192];
    snprintf(text, sizeof(text), "%s", header);
    map_text(&m, text + strlen(text), sizeof(text) - strlen(text));
    memset(e, 0, sizeof(*e));
    SimConfig cfg = {.level_text = text, .seed = 1};
    char err[256] = "";
    if (!sim_init(&e->sim, &cfg, err, sizeof(err))) {
        fprintf(stderr, "sim_init failed: %s\n", err);
        return false;
    }
    return true;
}

static void tick_n(Env *e, int n) {
    for (int i = 0; i < n; i++) {
        input_frame_update(&e->in, 0);
        sim_tick(&e->sim, &e->in);
    }
}

// Places a resting crate in a pad cell (col 0..3, row 0 = top).
static EntityId put(Env *e, int col, int row, CrateKind kind) {
    const PadSite *s = &e->sim.level.pads[0];
    const int cx = s->cell_x + col, cy = s->cell_y - (PAD_ROWS - 1) + row;
    return crate_spawn(&e->sim.gs, kind, vec2((f32)(cx * TILE_SIZE), (f32)(cy * TILE_SIZE)), CRATE_RESTING, -1);
}

static u32 products(Env *e) {
    u32 n = 0;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        n += e->sim.gs.entities.slots[i].type == ENT_PRODUCT ? 1u : 0u;
    }
    return n;
}

static const Product *first_product(Env *e) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        if (e->sim.gs.entities.slots[i].type == ENT_PRODUCT) {
            return &e->sim.gs.entities.slots[i].as.product;
        }
    }
    return NULL;
}

static void test_reads_the_crates_of_the_pad(void) {
    Env e;
    CHECK(env_init(&e, "", true));
    CHECK(e.sim.level.pads[0].present && !e.sim.level.pads[1].present);
    CHECK(e.sim.level.pads[0].cell_x == 6 && e.sim.level.pads[0].cell_y == 9);
    Rect r = pad_rect(&e.sim.gs, 0);
    CHECK_NEAR(r.x, 96.0, 0.0);
    CHECK_NEAR(r.y, 128.0, 0.0); // two rows up from the marker
    CHECK_NEAR(r.w, 64.0, 0.0);
    CHECK_NEAR(r.h, 32.0, 0.0);

    put(&e, 1, 1, CRATE_RED);
    put(&e, 1, 0, CRATE_BLUE);
    put(&e, 3, 1, CRATE_RED);
    // Crates outside the 4x2 area do not count: left of the pad, and a third one stacked on top.
    crate_spawn(&e.sim.gs, CRATE_RED, vec2(5 * 16, 9 * 16), CRATE_RESTING, -1);
    crate_spawn(&e.sim.gs, CRATE_RED, vec2(7 * 16, 7 * 16), CRATE_RESTING, -1);
    PadGrid g;
    EntityId ids[PAD_ROWS][PAD_COLS];
    CHECK(pad_read_grid(&e.sim.gs, 0, &g, ids));
    CHECK(g.cells[1][1] == PAD_RED && g.cells[0][1] == PAD_BLUE && g.cells[1][3] == PAD_RED);
    CHECK(g.cells[0][0] == PAD_EMPTY && g.cells[1][0] == PAD_EMPTY && g.cells[0][3] == PAD_EMPTY);
    CHECK(entity_id_valid(ids[1][1]) && !entity_id_valid(ids[0][0]));
    CHECK(!pad_read_grid(&e.sim.gs, 1, &g, NULL)); // team 2 has no pad here
    sim_free(&e.sim);
}

static void test_build_consumes_from_the_top_and_makes_a_product(void) {
    Env e;
    CHECK(env_init(&e, "", true));
    EntityId bottom = put(&e, 1, 1, CRATE_RED);
    EntityId top = put(&e, 1, 0, CRATE_RED);
    GameState *gs = &e.sim.gs;

    PadOptionList l;
    pad_world_options(gs, 0, &l);
    CHECK(find(&l, SHAPE_2X1_V, false) != NULL);

    // A 1x1 uses the top crate only; the bottom one stays.
    CHECK(pad_build(gs, 0, SHAPE_1X1, false));
    CHECK(pool_get(&gs->entities, top)->pending_destroy);
    CHECK(!pool_get(&gs->entities, bottom)->pending_destroy);
    CHECK_EQ_U64(gs->crate_stats.consumed, 1);
    CHECK_EQ_U64(gs->crate_stats.destroyed, 0);
    CHECK_EQ_U64(gs->products_built, 1);
    CHECK_EQ_U64(products(&e), 1);
    CHECK(first_product(&e)->kind == PRODUCT_PISTOL && first_product(&e)->team == 0);
    CHECK_EQ_U64(events_count_of(&gs->events, EV_PAD_BUILT), 1);
    CHECK_EQ_U64(events_count_of(&gs->events, EV_CRATE_CONSUMED), 1);
    tick_n(&e, 1); // flush
    CHECK(pool_get(&gs->entities, top) == NULL);
    CHECK_EQ_U64(crate_count(gs), 1);

    // The shape is gone: the same request is now refused and changes nothing.
    CHECK(pad_build(gs, 0, SHAPE_2X1_V, false) == false);
    CHECK(pad_build(gs, 0, SHAPE_1X1, true) == false); // no blue crate
    CHECK_EQ_U64(gs->products_built, 1);
    sim_free(&e.sim);
}

static void test_build_blue_version_and_stale_menu(void) {
    Env e;
    CHECK(env_init(&e, "", true));
    put(&e, 0, 1, CRATE_RED);
    put(&e, 1, 1, CRATE_BLUE);
    GameState *gs = &e.sim.gs;
    CHECK(!pad_build(gs, 0, SHAPE_2X1_H, false)); // the pair has a blue crate: only the blue version
    CHECK(pad_build(gs, 0, SHAPE_2X1_H, true));
    CHECK(first_product(&e)->kind == PRODUCT_STRONG_GRENADES);
    CHECK_EQ_U64(gs->crate_stats.consumed, 2);
    sim_free(&e.sim);
}

static void test_build_every_product_in_the_world(void) {
    struct {
        PadShape shape;
        bool blue;
        const char *rows[2];
        PadProduct expect;
    } cases[] = {
        {SHAPE_1X1, false, {"....", "r..."}, PRODUCT_PISTOL},
        {SHAPE_1X1, true, {"....", "b..."}, PRODUCT_UZI},
        {SHAPE_2X1_H, false, {"....", "rr.."}, PRODUCT_GRENADES},
        {SHAPE_2X1_V, true, {"r...", "b..."}, PRODUCT_STRONG_COMPANION},
        {SHAPE_L, false, {"r...", "rr.."}, PRODUCT_UZI},
        {SHAPE_L, true, {"..r.", ".rb."}, PRODUCT_ROCKET_LAUNCHER},
        {SHAPE_2X2, false, {".rr.", ".rr."}, PRODUCT_HEAVY_COMPANION},
        {SHAPE_2X2, true, {"rr..", "rb.."}, PRODUCT_HEAVY_ARMORED_COMPANION},
        {SHAPE_4X2, false, {"rrrr", "rrrr"}, PRODUCT_TANK},
        {SHAPE_4X2, true, {"rrrr", "rrrb"}, PRODUCT_HEAVY_TANK},
    };
    for (u32 i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        Env e;
        CHECK(env_init(&e, "", true));
        u32 n = 0;
        for (int r = 0; r < 2; r++) {
            for (int c = 0; c < 4; c++) {
                const char ch = cases[i].rows[r][c];
                if (ch != '.') {
                    put(&e, c, r, ch == 'b' ? CRATE_BLUE : CRATE_RED);
                    n++;
                }
            }
        }
        CHECK(pad_build(&e.sim.gs, 0, cases[i].shape, cases[i].blue));
        CHECK(first_product(&e)->kind == cases[i].expect);
        CHECK_EQ_U64(e.sim.gs.crate_stats.consumed, n);
        tick_n(&e, 1);
        CHECK_EQ_U64(crate_count(&e.sim.gs), 0);
        sim_free(&e.sim);
    }
}

static void test_no_pad_no_build(void) {
    Env e;
    CHECK(env_init(&e, "", false));
    PadOptionList l;
    pad_world_options(&e.sim.gs, 0, &l);
    CHECK_EQ_U64(l.count, 0);
    CHECK(!pad_usable(&e.sim.gs, 0));
    CHECK(!pad_build(&e.sim.gs, 0, SHAPE_1X1, false));
    CHECK(!pad_build(&e.sim.gs, 3, SHAPE_1X1, false)); // another team
    CHECK(!pad_build(&e.sim.gs, -1, SHAPE_1X1, false));
    CHECK(!pad_build(&e.sim.gs, 0, SHAPE_COUNT, false));
    sim_free(&e.sim);
}

static void test_full_pool_changes_nothing(void) {
    Env e;
    CHECK(env_init(&e, "", true));
    put(&e, 0, 1, CRATE_RED);
    GameState *gs = &e.sim.gs;
    while (entity_id_valid(pool_create(&gs->entities, ENT_PRODUCT))) {
    }
    const u32 before = gs->products_built;
    CHECK(!pad_build(gs, 0, SHAPE_1X1, false));
    CHECK_EQ_U64(gs->products_built, before);
    CHECK_EQ_U64(gs->crate_stats.consumed, 0);
    CHECK_EQ_U64(crate_count(gs), 1); // the crate is still there
    sim_free(&e.sim);
}

static void test_product_goes_to_the_other_side_when_blocked(void) {
    // Wall of solid tiles left of the pad: the product appears on its right instead.
    TestMap m;
    map_init(&m, 20, 12);
    map_rect(&m, 0, 10, 19, 11, '#');
    map_rect(&m, 5, 6, 5, 9, '#');
    map_put(&m, 1, 9, '1');
    map_put(&m, 6, 9, 'P');
    char text[4096] = "";
    map_text(&m, text, sizeof(text));
    Sim sim;
    SimConfig cfg = {.level_text = text, .seed = 1};
    char err[128];
    CHECK(sim_init(&sim, &cfg, err, sizeof(err)));
    pad_debug_fill(&sim.gs, 0, "....", "r...");
    CHECK(pad_build(&sim.gs, 0, SHAPE_1X1, false));
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        if (sim.gs.entities.slots[i].type == ENT_PRODUCT) {
            CHECK(sim.gs.entities.slots[i].body.pos.x >= 10 * 16);
        }
    }
    sim_free(&sim);
}

static void test_debug_fill(void) {
    Env e;
    CHECK(env_init(&e, "", true));
    CHECK(!pad_debug_fill(&e.sim.gs, 0, "rr.", "...."));    // wrong length
    CHECK(!pad_debug_fill(&e.sim.gs, 0, "rr.x", "...."));   // wrong character
    CHECK(!pad_debug_fill(&e.sim.gs, 1, "....", "...."));   // no pad for team 2
    CHECK_EQ_U64(crate_count(&e.sim.gs), 0);
    CHECK(pad_debug_fill(&e.sim.gs, 0, ".b..", "rrr."));
    CHECK_EQ_U64(crate_count(&e.sim.gs), 4);
    PadGrid g;
    pad_read_grid(&e.sim.gs, 0, &g, NULL);
    CHECK(g.cells[0][1] == PAD_BLUE && g.cells[1][0] == PAD_RED && g.cells[1][2] == PAD_RED);
    CHECK(g.cells[0][0] == PAD_EMPTY && g.cells[1][3] == PAD_EMPTY);
    sim_free(&e.sim);
}

static void test_product_falls_and_rests_beside_the_pad(void) {
    Env e;
    CHECK(env_init(&e, "", true));
    put(&e, 0, 1, CRATE_RED);
    CHECK(pad_build(&e.sim.gs, 0, SHAPE_1X1, false));
    tick_n(&e, 90);
    const Entity *p = NULL;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        if (e.sim.gs.entities.slots[i].type == ENT_PRODUCT) {
            p = &e.sim.gs.entities.slots[i];
        }
    }
    CHECK(p != NULL);
    CHECK(p->body.on_ground);
    CHECK_NEAR(p->body.pos.y + p->body.size.y, 160.0, 0.01); // on the floor
    CHECK(p->body.pos.x + p->body.size.x <= 6 * 16);         // left of the pad (columns 6-9)
    sim_free(&e.sim);
}

static void test_product_is_not_buried_by_a_crate(void) {
    Env e;
    CHECK(env_init(&e, "", true));
    put(&e, 0, 1, CRATE_RED);
    CHECK(pad_build(&e.sim.gs, 0, SHAPE_1X1, false));
    tick_n(&e, 60); // the product rests on the floor, left of the pad (column 5)
    // A crate falls onto the very same cell and settles there.
    crate_spawn(&e.sim.gs, CRATE_RED, vec2(5 * 16, 5 * 16), CRATE_FALLING, -1);
    tick_n(&e, 90);
    const Entity *p = NULL;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        if (e.sim.gs.entities.slots[i].type == ENT_PRODUCT) {
            p = &e.sim.gs.entities.slots[i];
        }
    }
    CHECK(p != NULL);
    const Vec2 c = body_center(&p->body);
    CHECK(!level_is_solid(&e.sim.level, level_px_to_tile(c.x), level_px_to_tile(c.y)));
    CHECK(p->body.pos.y + p->body.size.y <= 9 * 16 + 0.01f); // on top of the crate (row 9), not inside it
    sim_free(&e.sim);
}

static void test_removing_consumed_crates_lets_the_rest_fall(void) {
    Env e;
    CHECK(env_init(&e, "", true));
    EntityId bottom = put(&e, 2, 1, CRATE_RED);
    EntityId top = put(&e, 2, 0, CRATE_RED);
    // A 2x1 vertical consumes both; with three stacked, the third (outside the pad) falls on the floor.
    EntityId third = crate_spawn(&e.sim.gs, CRATE_RED, vec2(8 * 16, 7 * 16), CRATE_RESTING, -1);
    CHECK(pad_build(&e.sim.gs, 0, SHAPE_2X1_V, false));
    tick_n(&e, 60);
    CHECK(pool_get(&e.sim.gs.entities, top) == NULL && pool_get(&e.sim.gs.entities, bottom) == NULL);
    const Entity *t = pool_get(&e.sim.gs.entities, third);
    CHECK(t != NULL && t->as.crate.state == CRATE_RESTING && t->as.crate.cell_y == 9);
    sim_free(&e.sim);
}

static void test_deterministic_world(void) {
    u64 h[2];
    for (int run = 0; run < 2; run++) {
        Env e;
        CHECK(env_init(&e, "", true));
        put(&e, 0, 1, CRATE_RED);
        put(&e, 1, 1, CRATE_BLUE);
        pad_build(&e.sim.gs, 0, SHAPE_2X1_H, true);
        tick_n(&e, 120);
        h[run] = game_hash(&e.sim.gs);
        sim_free(&e.sim);
    }
    CHECK(h[0] == h[1]);
}

int main(void) {
    RUN_TEST(test_empty_pad_offers_nothing);
    RUN_TEST(test_recipe_table);
    RUN_TEST(test_single_crate_everywhere);
    RUN_TEST(test_horizontal_pair);
    RUN_TEST(test_vertical_pair);
    RUN_TEST(test_l_both_orientations);
    RUN_TEST(test_two_by_two);
    RUN_TEST(test_full_pad_is_a_tank);
    RUN_TEST(test_blue_choice_when_both_exist);
    RUN_TEST(test_consumption_starts_from_the_top);
    RUN_TEST(test_options_are_ordered_biggest_first_red_before_blue);
    RUN_TEST(test_exhaustive_all_grids);
    RUN_TEST(test_reads_the_crates_of_the_pad);
    RUN_TEST(test_build_consumes_from_the_top_and_makes_a_product);
    RUN_TEST(test_build_blue_version_and_stale_menu);
    RUN_TEST(test_build_every_product_in_the_world);
    RUN_TEST(test_no_pad_no_build);
    RUN_TEST(test_full_pool_changes_nothing);
    RUN_TEST(test_debug_fill);
    RUN_TEST(test_product_falls_and_rests_beside_the_pad);
    RUN_TEST(test_product_goes_to_the_other_side_when_blocked);
    RUN_TEST(test_product_is_not_buried_by_a_crate);
    RUN_TEST(test_removing_consumed_crates_lets_the_rest_fall);
    RUN_TEST(test_deterministic_world);
    TEST_MAIN_END();
}
