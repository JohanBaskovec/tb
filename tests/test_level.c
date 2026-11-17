#include "test.h"

#include "game/level.h"

#include <stdlib.h>
#include <string.h>

#ifndef CS_ASSET_DIR
#error "CS_ASSET_DIR must be defined by CMake"
#endif

static bool parse(Level *lv, Arena *a, const char *text, char *err, usize err_len) {
    arena_reset(a);
    return level_parse(lv, a, text, err, err_len);
}

static void test_parse_basic(void) {
    Arena a;
    CHECK(arena_init(&a, 1 << 16));
    Level lv;
    char err[128] = "";
    const char *txt = "; a comment\nname=Tiny\n\n---\n#####\n#1.D#\n##=H#\n";
    CHECK(parse(&lv, &a, txt, err, sizeof(err)));
    CHECK_EQ_U64(lv.width, 5);
    CHECK_EQ_U64(lv.height, 3);
    CHECK(strcmp(lv.name, "Tiny") == 0);
    CHECK_EQ_U64(level_tile(&lv, 0, 0), TILE_SOLID);
    CHECK_EQ_U64(level_tile(&lv, 2, 2), TILE_PLATFORM);
    CHECK_EQ_U64(level_tile(&lv, 3, 2), TILE_LADDER);
    CHECK_EQ_U64(level_tile(&lv, 1, 1), TILE_EMPTY); // spawn marker leaves an empty tile
    CHECK(lv.has_spawn[0] && !lv.has_spawn[1]);
    // Spawn = bottom-center of its cell.
    CHECK_NEAR(lv.spawns[0].x, 1.5f * TILE_SIZE, 0.0);
    CHECK_NEAR(lv.spawns[0].y, 2.0f * TILE_SIZE, 0.0);
    CHECK_EQ_U64(lv.drop_zone_count, 1);
    CHECK_NEAR(lv.drop_zones[0].x, 3.5f * TILE_SIZE, 0.0);
    CHECK_NEAR(lv.drop_zones[0].y, 1.0f * TILE_SIZE, 0.0);
    arena_free(&a);
}

static void test_parse_crlf_and_trailing_blank_lines(void) {
    Arena a;
    CHECK(arena_init(&a, 1 << 16));
    Level lv;
    char err[128] = "";
    CHECK(parse(&lv, &a, "name=Win\r\n---\r\n##\r\n##\r\n\r\n\r\n", err, sizeof(err)));
    CHECK_EQ_U64(lv.width, 2);
    CHECK_EQ_U64(lv.height, 2);
    CHECK(strcmp(lv.name, "Win") == 0);
    arena_free(&a);
}

typedef struct ErrCase {
    const char *text;
    const char *expected; // substring of the error message
} ErrCase;

static void test_parse_errors(void) {
    static const ErrCase cases[] = {
        {"name=x\n", "missing '---'"},
        {"---\n", "empty"},
        {"---\n###\n##\n", "expected 3"},
        {"---\n#x#\n", "unknown tile 'x'"},
        {"---\n11\n", "duplicate spawn"},
        {"foo=bar\n---\n#\n", "unknown header key"},
        {"justtext\n---\n#\n", "key=value"},
        {"---\n#\n\n#\n", "blank line"},
        {"---\nDDDDDDDDDDDDDDDDD\n", "too many drop zones"},
    };
    Arena a;
    CHECK(arena_init(&a, 1 << 16));
    for (usize i = 0; i < ARRAY_LEN(cases); i++) {
        Level lv;
        char err[128] = "";
        bool ok = parse(&lv, &a, cases[i].text, err, sizeof(err));
        CHECK(!ok);
        if (strstr(err, cases[i].expected) == NULL) {
            fprintf(stderr, "case %d: error '%s' does not contain '%s'\n", (int)i, err,
                    cases[i].expected);
            g_failures++;
        }
    }

    // Too wide.
    char wide[MAX_LEVEL_W + 32];
    strcpy(wide, "---\n");
    memset(wide + 4, '#', MAX_LEVEL_W + 1);
    strcpy(wide + 4 + MAX_LEVEL_W + 1, "\n");
    Level lv;
    char err[128] = "";
    CHECK(!parse(&lv, &a, wide, err, sizeof(err)));
    CHECK(strstr(err, "maximum") != NULL);

    // Arena too small.
    Arena tiny;
    CHECK(arena_init(&tiny, 4));
    CHECK(!level_parse(&lv, &tiny, "---\n##########\n", err, sizeof(err)));
    CHECK(strstr(err, "arena") != NULL);
    arena_free(&tiny);

    // A NULL/zero-length error buffer must not crash.
    CHECK(!level_parse(&lv, &a, "---\n", NULL, 0));
    arena_free(&a);
}

static void test_drop_zone_config(void) {
    Arena a;
    CHECK(arena_init(&a, 1 << 16));
    Level lv;
    char err[128] = "";
    // Zones are numbered in reading order: D at (1,0), D at (3,0), D at (2,1).
    const char *txt = "drop.1=red,5,3\ndrop.3=mixed,10,4,30\n---\n.D.D.\n..D..\n";
    CHECK(parse(&lv, &a, txt, err, sizeof(err)));
    CHECK_EQ_U64(lv.drop_zone_count, 3);
    CHECK_EQ_U64(lv.drop_cfg[0].color, DROP_RED);
    CHECK_EQ_U64(lv.drop_cfg[0].interval_s, 5);
    CHECK_EQ_U64(lv.drop_cfg[0].cap, 3);
    // Zone 2 is not configured: defaults.
    CHECK_EQ_U64(lv.drop_cfg[1].color, DROP_MIXED);
    CHECK_EQ_U64(lv.drop_cfg[1].interval_s, DEFAULT_DROP_INTERVAL_S);
    CHECK_EQ_U64(lv.drop_cfg[1].cap, DEFAULT_DROP_CAP);
    CHECK_EQ_U64(lv.drop_cfg[1].blue_percent, DEFAULT_DROP_BLUE_PERCENT);
    CHECK_EQ_U64(lv.drop_cfg[2].color, DROP_MIXED);
    CHECK_EQ_U64(lv.drop_cfg[2].interval_s, 10);
    CHECK_EQ_U64(lv.drop_cfg[2].cap, 4);
    CHECK_EQ_U64(lv.drop_cfg[2].blue_percent, 30);
    // And the markers are where the numbering says.
    CHECK_NEAR(lv.drop_zones[0].x, 1.5f * TILE_SIZE, 0.0);
    CHECK_NEAR(lv.drop_zones[2].y, 1.0f * TILE_SIZE, 0.0);

    CHECK(parse(&lv, &a, "drop.1=blue,1,1\n---\nD\n", err, sizeof(err)));
    CHECK_EQ_U64(lv.drop_cfg[0].color, DROP_BLUE);
    arena_free(&a);
}

static void test_drop_zone_config_errors(void) {
    static const ErrCase cases[] = {
        {"drop.1=green,5,3\n---\nD\n", "unknown color"},
        {"drop.1=red,0,3\n---\nD\n", "interval"},
        {"drop.1=red,601,3\n---\nD\n", "interval"},
        {"drop.1=red,5,0\n---\nD\n", "cap"},
        {"drop.1=red,5,65\n---\nD\n", "cap"},
        {"drop.1=mixed,5,3,101\n---\nD\n", "blue percentage"},
        {"drop.1=red,5\n---\nD\n", "expects COLOR"},
        {"drop.1=red,5,3,10,9\n---\nD\n", "expects COLOR"},
        {"drop.1=\n---\nD\n", "expects COLOR"},
        {"drop.1=red,x,3\n---\nD\n", "interval"},
        {"drop.0=red,5,3\n---\nD\n", "zone number"},
        {"drop.17=red,5,3\n---\nD\n", "zone number"},
        {"drop.a=red,5,3\n---\nD\n", "zone number"},
        {"drop.1=red,5,3\ndrop.1=blue,5,3\n---\nD\n", "duplicate drop.1"},
        {"drop.2=red,5,3\n---\nD\n", "does not exist"},
        {"drop.1=red,5,3\n---\n.\n", "does not exist"},
    };
    Arena a;
    CHECK(arena_init(&a, 1 << 16));
    for (usize i = 0; i < ARRAY_LEN(cases); i++) {
        Level lv;
        char err[160] = "";
        bool ok = parse(&lv, &a, cases[i].text, err, sizeof(err));
        CHECK(!ok);
        if (strstr(err, cases[i].expected) == NULL) {
            fprintf(stderr, "case %d: error '%s' does not contain '%s'\n", (int)i, err,
                    cases[i].expected);
            g_failures++;
        }
    }
    arena_free(&a);
}

static void test_pad_markers(void) {
    Arena a;
    CHECK(arena_init(&a, 1 << 16));
    Level lv;
    char err[128] = "";
    // The N-th marker in reading order belongs to team N by default; each marks a bottom-left cell.
    CHECK(parse(&lv, &a, "---\n..........\n.P...P....\n##########\n", err, sizeof(err)));
    CHECK(lv.pads[0].present && lv.pads[1].present && !lv.pads[2].present && !lv.pads[3].present);
    CHECK(lv.pads[0].cell_x == 1 && lv.pads[0].cell_y == 1);
    CHECK(lv.pads[1].cell_x == 5 && lv.pads[1].cell_y == 1);
    CHECK_EQ_U64(level_tile(&lv, 1, 1), TILE_EMPTY); // the marker leaves an empty tile

    // Not enough room: 4 columns to the right, 1 more row above.
    CHECK(!parse(&lv, &a, "---\n........\n.....P..\n########\n", err, sizeof(err)));
    CHECK(strstr(err, "does not fit") != NULL);
    CHECK(!parse(&lv, &a, "---\nP.......\n########\n", err, sizeof(err)));
    CHECK(strstr(err, "does not fit") != NULL);
    arena_free(&a);
}

static void test_pad_owner_header_and_errors(void) {
    Arena a;
    CHECK(arena_init(&a, 1 << 16));
    Level lv;
    char err[128] = "";
    const char *map = "---\n..........\n.P....P...\n##########\n";
    char text[256];

    snprintf(text, sizeof(text), "pad.1=3\npad.2=2\n%s", map);
    CHECK(parse(&lv, &a, text, err, sizeof(err)));
    CHECK(lv.pads[2].present && lv.pads[2].cell_x == 1); // first marker -> team 3
    CHECK(lv.pads[1].present && lv.pads[1].cell_x == 6); // second marker -> team 2
    CHECK(!lv.pads[0].present);

    snprintf(text, sizeof(text), "pad.1=2\n%s", map); // marker 1 -> team 2, marker 2 defaults to team 2
    CHECK(!parse(&lv, &a, text, err, sizeof(err)));
    CHECK(strstr(err, "already has a pad") != NULL);

    const char *bad[] = {"pad.1=9", "pad.1=0", "pad.0=1", "pad.5=1", "pad.x=1", "pad.1=", "pad.1=a"};
    for (u32 i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        snprintf(text, sizeof(text), "%s\n%s", bad[i], map);
        CHECK(!parse(&lv, &a, text, err, sizeof(err)));
        CHECK(strstr(err, "pad.N=TEAM") != NULL);
    }
    arena_free(&a);
}

static void test_dynamic_solid_overlay(void) {
    Arena a;
    CHECK(arena_init(&a, 1 << 16));
    Level lv;
    char err[128];
    CHECK(parse(&lv, &a, "---\n...\n###\n", err, sizeof(err)));
    CHECK(!level_is_solid(&lv, 1, 0));
    level_set_dyn_solid(&lv, 1, 0, true);
    CHECK(level_is_solid(&lv, 1, 0));   // a resting crate is solid like terrain...
    CHECK(level_dyn_solid(&lv, 1, 0));
    CHECK_EQ_U64(level_tile(&lv, 1, 0), TILE_EMPTY); // ...but the static map is untouched
    CHECK(!level_is_one_way(&lv, 1, 0));
    level_set_dyn_solid(&lv, 1, 0, false);
    CHECK(!level_is_solid(&lv, 1, 0));
    CHECK(!level_dyn_solid(&lv, -1, 0)); // out of range: no crate (walls are solid through level_tile)
    CHECK(level_is_solid(&lv, -1, 0));
    arena_free(&a);
}

static void test_out_of_bounds_tiles(void) {
    Arena a;
    CHECK(arena_init(&a, 1 << 16));
    Level lv;
    char err[128];
    CHECK(parse(&lv, &a, "---\n...\n...\n", err, sizeof(err)));
    CHECK_EQ_U64(level_tile(&lv, -1, 0), TILE_SOLID); // left wall
    CHECK_EQ_U64(level_tile(&lv, 3, 1), TILE_SOLID);  // right wall
    CHECK_EQ_U64(level_tile(&lv, 1, -1), TILE_SOLID); // ceiling
    CHECK_EQ_U64(level_tile(&lv, 1, 2), TILE_HAZARD); // void below
    CHECK_EQ_U64(level_tile(&lv, -1, 5), TILE_SOLID); // walls win over the void
    CHECK_EQ_U64(level_tile_at_px(&lv, 17.5f, 3.0f), level_tile(&lv, 1, 0));
    CHECK_EQ_U64(level_px_to_tile(-0.5f), -1); // floor, not truncation
    arena_free(&a);
}

static void test_one_way_rules(void) {
    Arena a;
    CHECK(arena_init(&a, 1 << 16));
    Level lv;
    char err[128];
    // Column 0: ladder of 3 cells (top one is a one-way platform). Column 1: thin platform.
    CHECK(parse(&lv, &a, "---\nH=\nH.\nH#\n", err, sizeof(err)));
    CHECK(level_is_one_way(&lv, 0, 0));  // ladder top
    CHECK(!level_is_one_way(&lv, 0, 1)); // ladder middle
    CHECK(!level_is_one_way(&lv, 0, 2));
    CHECK(level_is_one_way(&lv, 1, 0)); // platform
    CHECK(!level_is_one_way(&lv, 1, 2));
    CHECK(level_is_solid(&lv, 1, 2));
    CHECK(!level_is_solid(&lv, 1, 0));
    arena_free(&a);
}

static void test_real_level_file(void) {
    Arena a;
    CHECK(arena_init(&a, 1 << 20));
    Level lv;
    char err[256] = "";
    CHECK(level_load_file(&lv, &a, CS_ASSET_DIR "/levels/test_arena.lvl", err, sizeof(err)));
    if (err[0]) {
        fprintf(stderr, "%s\n", err);
    }
    CHECK_EQ_U64(lv.width, 80);
    CHECK_EQ_U64(lv.height, 30);
    CHECK(strcmp(lv.name, "Test Arena") == 0);
    for (int t = 0; t < MAX_TEAMS; t++) {
        CHECK(lv.has_spawn[t]);
        // Every spawn has solid ground (or a one-way top) right under its feet.
        i32 tx = level_px_to_tile(lv.spawns[t].x);
        i32 ty = level_px_to_tile(lv.spawns[t].y + 0.5f);
        CHECK(level_is_solid(&lv, tx, ty) || level_is_one_way(&lv, tx, ty));
        // And the cell of the body itself is free.
        CHECK_EQ_U64(level_tile(&lv, tx, ty - 1), TILE_EMPTY);
    }
    CHECK_EQ_U64(lv.drop_zone_count, 3);
    // Red near each base, blue only at the centre.
    CHECK_EQ_U64(lv.drop_cfg[0].color, DROP_RED);
    CHECK_EQ_U64(lv.drop_cfg[1].color, DROP_BLUE);
    CHECK_EQ_U64(lv.drop_cfg[2].color, DROP_RED);
    CHECK(lv.drop_zones[0].x < lv.drop_zones[1].x && lv.drop_zones[1].x < lv.drop_zones[2].x);
    for (int z = 0; z < 3; z++) {
        // Each zone has open sky around its marker cell.
        CHECK(!level_is_solid(&lv, level_px_to_tile(lv.drop_zones[z].x), level_px_to_tile(lv.drop_zones[z].y)));
    }

    // Pads of teams 1 and 2: 4 columns x 2 rows, free air above solid ground.
    for (int t = 0; t < 2; t++) {
        CHECK(lv.pads[t].present);
        for (int c = 0; c < 4; c++) {
            const i32 x = lv.pads[t].cell_x + c, y = lv.pads[t].cell_y;
            CHECK(level_is_solid(&lv, x, y + 1));
            CHECK(!level_is_solid(&lv, x, y) && !level_is_solid(&lv, x, y - 1));
        }
    }
    CHECK(!lv.pads[2].present && !lv.pads[3].present);
    CHECK(lv.pads[0].cell_x < lv.pads[1].cell_x);

    CHECK(!level_load_file(&lv, &a, CS_ASSET_DIR "/levels/does_not_exist.lvl", err, sizeof(err)));
    CHECK(strstr(err, "cannot open") != NULL);
    arena_free(&a);
}

int main(void) {
    RUN_TEST(test_parse_basic);
    RUN_TEST(test_parse_crlf_and_trailing_blank_lines);
    RUN_TEST(test_parse_errors);
    RUN_TEST(test_drop_zone_config);
    RUN_TEST(test_drop_zone_config_errors);
    RUN_TEST(test_pad_markers);
    RUN_TEST(test_pad_owner_header_and_errors);
    RUN_TEST(test_dynamic_solid_overlay);
    RUN_TEST(test_out_of_bounds_tiles);
    RUN_TEST(test_one_way_rules);
    RUN_TEST(test_real_level_file);
    TEST_MAIN_END();
}
