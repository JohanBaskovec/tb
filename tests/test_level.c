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

    CHECK(!level_load_file(&lv, &a, CS_ASSET_DIR "/levels/does_not_exist.lvl", err, sizeof(err)));
    CHECK(strstr(err, "cannot open") != NULL);
    arena_free(&a);
}

int main(void) {
    RUN_TEST(test_parse_basic);
    RUN_TEST(test_parse_crlf_and_trailing_blank_lines);
    RUN_TEST(test_parse_errors);
    RUN_TEST(test_out_of_bounds_tiles);
    RUN_TEST(test_one_way_rules);
    RUN_TEST(test_real_level_file);
    TEST_MAIN_END();
}
