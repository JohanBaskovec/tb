#include "test.h"

#include "game/physics.h"
#include "game/tuning.h"
#include "test_helpers.h"

// 12x8 map, floor at row 5 (top at y = 80).
#define FLOOR_TOP 80.0f
#define BODY_W 12.0f
#define BODY_H 22.0f

static Body make_body(f32 x, f32 y) {
    Body b = {0};
    b.pos = vec2(x, y);
    b.size = vec2(BODY_W, BODY_H);
    return b;
}

// One tick with gravity, like a companion would do.
static void fall_tick(Body *b, const Level *lv, bool ignore_one_way) {
    b->vel.y = MIN(b->vel.y + TUNING_GRAVITY * TICK_DT, TUNING_MAX_FALL_SPEED);
    body_move(b, lv, TICK_DT, ignore_one_way);
}

static void test_lands_on_floor(void) {
    TestMap m;
    map_init(&m, 12, 8);
    map_rect(&m, 0, 5, 11, 7, '#');
    Arena a;
    Level lv;
    CHECK(arena_init(&a, 1 << 16) && map_load(&m, &lv, &a));
    Body b = make_body(40, 0);
    for (int i = 0; i < 120; i++) {
        fall_tick(&b, &lv, false);
    }
    CHECK(b.on_ground);
    CHECK_NEAR(b.pos.y, FLOOR_TOP - BODY_H, 0.0);
    CHECK_NEAR(b.vel.y, 0.0, 0.0);
    arena_free(&a);
}

static void test_no_tunneling_at_max_fall_speed(void) {
    TestMap m;
    map_init(&m, 12, 8);
    map_rect(&m, 0, 5, 11, 5, '#'); // a single 1-tile-thick floor
    Arena a;
    Level lv;
    CHECK(arena_init(&a, 1 << 16) && map_load(&m, &lv, &a));
    // Every sub-tile starting offset must still land on top.
    for (int i = 0; i < 40; i++) {
        Body b = make_body(40, 3.0f + (f32)i * 0.37f);
        b.vel.y = TUNING_MAX_FALL_SPEED;
        for (int t = 0; t < 30; t++) {
            fall_tick(&b, &lv, false);
            CHECK(b.pos.y + BODY_H <= FLOOR_TOP + 0.001f);
        }
        CHECK(b.on_ground);
        CHECK_NEAR(b.pos.y, FLOOR_TOP - BODY_H, 0.0);
    }
    arena_free(&a);
}

static void test_walls(void) {
    TestMap m;
    map_init(&m, 12, 8);
    map_rect(&m, 0, 5, 11, 7, '#');
    map_rect(&m, 8, 0, 8, 4, '#'); // wall on the right, x in [128,144)
    map_rect(&m, 1, 0, 1, 4, '#'); // wall on the left, x in [16,32)
    Arena a;
    Level lv;
    CHECK(arena_init(&a, 1 << 16) && map_load(&m, &lv, &a));

    // hit_wall is only set on the tick of the collision (afterwards vel.x is 0).
    Body b = make_body(80, FLOOR_TOP - BODY_H);
    b.vel.x = TUNING_WALK_SPEED;
    bool hit = false;
    for (int i = 0; i < 120; i++) {
        b.vel.x = TUNING_WALK_SPEED;
        body_move(&b, &lv, TICK_DT, false);
        hit = hit || b.hit_wall;
    }
    CHECK(hit);
    CHECK_NEAR(b.pos.x, 128.0f - BODY_W, 0.0);
    CHECK_NEAR(b.vel.x, 0.0, 0.0);

    Body c = make_body(80, FLOOR_TOP - BODY_H);
    hit = false;
    for (int i = 0; i < 120; i++) {
        c.vel.x = -TUNING_WALK_SPEED;
        body_move(&c, &lv, TICK_DT, false);
        hit = hit || c.hit_wall;
    }
    CHECK(hit);
    CHECK_NEAR(c.pos.x, 32.0f, 0.0);
    arena_free(&a);
}

static void test_map_borders_are_walls(void) {
    TestMap m;
    map_init(&m, 12, 8);
    map_rect(&m, 0, 5, 11, 7, '#');
    Arena a;
    Level lv;
    CHECK(arena_init(&a, 1 << 16) && map_load(&m, &lv, &a));
    Body b = make_body(80, FLOOR_TOP - BODY_H);
    b.vel.x = -TUNING_WALK_SPEED;
    for (int i = 0; i < 200; i++) {
        body_move(&b, &lv, TICK_DT, false);
    }
    CHECK_NEAR(b.pos.x, 0.0, 0.0);
    b.vel.x = TUNING_WALK_SPEED;
    for (int i = 0; i < 400; i++) {
        body_move(&b, &lv, TICK_DT, false);
    }
    CHECK_NEAR(b.pos.x, 12.0f * TILE_SIZE - BODY_W, 0.0);
    arena_free(&a);
}

static void test_ceiling(void) {
    TestMap m;
    map_init(&m, 12, 8);
    map_rect(&m, 0, 5, 11, 7, '#');
    map_rect(&m, 0, 2, 11, 2, '#'); // ceiling: bottom edge at y = 48
    Arena a;
    Level lv;
    CHECK(arena_init(&a, 1 << 16) && map_load(&m, &lv, &a));
    Body b = make_body(40, FLOOR_TOP - BODY_H);
    b.vel.y = -TUNING_JUMP_SPEED;
    bool bumped = false;
    for (int i = 0; i < 40; i++) {
        fall_tick(&b, &lv, false);
        if (b.hit_ceiling) {
            bumped = true;
            CHECK_NEAR(b.pos.y, 48.0, 0.0);
            CHECK(b.vel.y >= 0.0f);
        }
        CHECK(b.pos.y >= 48.0f);
    }
    CHECK(bumped);
    arena_free(&a);
}

static void test_one_way_platform(void) {
    TestMap m;
    map_init(&m, 12, 8);
    map_rect(&m, 0, 5, 11, 7, '#');
    map_rect(&m, 3, 3, 8, 3, '='); // thin platform, top at y = 48
    Arena a;
    Level lv;
    CHECK(arena_init(&a, 1 << 16) && map_load(&m, &lv, &a));
    const f32 plat_top = 48.0f;

    // From below: passes through and lands on top.
    Body up = make_body(80, FLOOR_TOP - BODY_H);
    up.vel.y = -TUNING_JUMP_SPEED;
    f32 min_y = up.pos.y;
    for (int i = 0; i < 120; i++) {
        fall_tick(&up, &lv, false);
        min_y = MIN(min_y, up.pos.y);
    }
    CHECK(min_y < plat_top - BODY_H); // went above the platform
    CHECK(up.on_ground);
    CHECK_NEAR(up.pos.y, plat_top - BODY_H, 0.0); // ended up standing on it

    // Standing on it: stable, and it counts as "one-way only" ground.
    for (int i = 0; i < 30; i++) {
        fall_tick(&up, &lv, false);
        CHECK_NEAR(up.pos.y, plat_top - BODY_H, 0.0);
    }
    CHECK(body_standing_on_one_way_only(&up, &lv));

    // From above while ignoring one-way tiles: falls through to the floor.
    Body down = make_body(80, plat_top - BODY_H);
    down.vel.y = TUNING_DROP_VY;
    for (int i = 0; i < 120; i++) {
        fall_tick(&down, &lv, true);
    }
    CHECK(down.on_ground);
    CHECK_NEAR(down.pos.y, FLOOR_TOP - BODY_H, 0.0);
    CHECK(!body_standing_on_one_way_only(&down, &lv)); // solid floor
    arena_free(&a);
}

static void test_hazard_overlap(void) {
    TestMap m;
    map_init(&m, 12, 8);
    map_rect(&m, 0, 5, 11, 7, '#');
    map_rect(&m, 5, 5, 7, 7, '~'); // water pit x in [80,128)
    Arena a;
    Level lv;
    CHECK(arena_init(&a, 1 << 16) && map_load(&m, &lv, &a));
    Body in = make_body(90, 70);
    CHECK(body_overlaps_tile(&in, &lv, TILE_HAZARD));
    Body edge = make_body(80 - BODY_W, FLOOR_TOP - BODY_H); // flush with the pit edge, on the floor
    CHECK(!body_overlaps_tile(&edge, &lv, TILE_HAZARD));
    Body void_below = make_body(40, 8 * 16 + 5.0f); // fell out of the bottom of the map
    CHECK(body_overlaps_tile(&void_below, &lv, TILE_HAZARD));
    arena_free(&a);
}

int main(void) {
    RUN_TEST(test_lands_on_floor);
    RUN_TEST(test_no_tunneling_at_max_fall_speed);
    RUN_TEST(test_walls);
    RUN_TEST(test_map_borders_are_walls);
    RUN_TEST(test_ceiling);
    RUN_TEST(test_one_way_platform);
    RUN_TEST(test_hazard_overlap);
    TEST_MAIN_END();
}
