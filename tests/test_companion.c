#include "test.h"

#include "game/companion.h"
#include "game/tuning.h"
#include "test_helpers.h"

// "Yard": 30x14 tiles. Floor top at y = 192. Features:
//   wall      col 4,  rows 10-11 (2 tiles high)
//   platform  cols 8-12, row 10 (top y = 160)
//   ladder    col 20, rows 7-11, ending flush with a slab (cols 21-29, rows 7-8; top y = 112)
#define YARD_FLOOR 192.0f
#define YARD_PLATFORM_TOP 160.0f
#define YARD_SLAB_TOP 112.0f
#define LADDER_X (20.0f * TILE_SIZE + 8.0f)
#define OPEN_X (15.0f * TILE_SIZE + 8.0f) // free column: nothing above, nothing near

typedef struct Yard {
    Arena arena;
    Level lv;
} Yard;

static bool yard_init(Yard *y) {
    TestMap m;
    map_init(&m, 30, 14);
    map_rect(&m, 0, 12, 29, 13, '#');
    map_rect(&m, 4, 10, 4, 11, '#');
    map_rect(&m, 8, 10, 12, 10, '=');
    map_rect(&m, 20, 7, 20, 11, 'H');
    map_rect(&m, 21, 7, 29, 8, '#');
    return arena_init(&y->arena, 1 << 16) && map_load(&m, &y->lv, &y->arena);
}

// A companion standing still at (x, feet_y), settled on the ground.
static Companion settled(const Yard *y, f32 x, f32 feet_y, InputFrame *in) {
    Companion c;
    companion_init(&c, vec2(x, feet_y));
    memset(in, 0, sizeof(*in));
    run_ticks(&c, &y->lv, in, 0, 3);
    return c;
}

static void test_walk_accel_and_cap(void) {
    Yard y;
    CHECK(yard_init(&y));
    InputFrame in;
    Companion c = settled(&y, OPEN_X, YARD_FLOOR, &in);
    CHECK(c.body.on_ground);

    run_ticks(&c, &y.lv, &in, KEY(ACT_RIGHT), 1);
    CHECK_NEAR(c.body.vel.x, TUNING_GROUND_ACCEL * TICK_DT, 1e-3); // accelerates, no instant speed
    run_ticks(&c, &y.lv, &in, KEY(ACT_RIGHT), 60);
    CHECK_NEAR(c.body.vel.x, TUNING_WALK_SPEED, 1e-3); // capped
    CHECK_EQ_U64(c.facing, 1);

    run_ticks(&c, &y.lv, &in, 0, 6); // 100 px/s decelerating at 1200 px/s² needs 5 ticks
    CHECK_NEAR(c.body.vel.x, 0.0, 0.0);
    arena_free(&y.arena);
}

static void test_facing_follows_input(void) {
    Yard y;
    CHECK(yard_init(&y));
    InputFrame in;
    Companion c = settled(&y, OPEN_X, YARD_FLOOR, &in);
    run_ticks(&c, &y.lv, &in, KEY(ACT_LEFT), 2);
    CHECK(c.facing == -1);
    run_ticks(&c, &y.lv, &in, 0, 30);
    CHECK(c.facing == -1); // idle keeps the last facing
    run_ticks(&c, &y.lv, &in, KEY(ACT_RIGHT), 2);
    CHECK(c.facing == 1);
    arena_free(&y.arena);
}

static void test_wall_blocks_walking(void) {
    Yard y;
    CHECK(yard_init(&y));
    InputFrame in;
    Companion c = settled(&y, 7.0f * TILE_SIZE + 8.0f, YARD_FLOOR, &in);
    run_ticks(&c, &y.lv, &in, KEY(ACT_LEFT), 150);
    CHECK_NEAR(c.body.pos.x, 5.0f * TILE_SIZE, 0.0); // wall at col 4 ends at x = 80
    arena_free(&y.arena);
}

// Highest rise (in px) reached while holding jump for `hold` ticks, then waiting for the landing.
static f32 jump_rise(const Yard *y, int hold) {
    InputFrame in;
    Companion c = settled(y, OPEN_X, YARD_FLOOR, &in);
    const f32 start = c.body.pos.y;
    f32 top = start;
    for (int i = 0; i < 90; i++) {
        input_frame_update(&in, i < hold ? KEY(ACT_JUMP) : 0);
        companion_update(&c, &y->lv, &in);
        top = MIN(top, c.body.pos.y);
    }
    CHECK(c.body.on_ground); // landed again
    return start - top;
}

static void test_variable_jump_height(void) {
    Yard y;
    CHECK(yard_init(&y));
    const f32 full = jump_rise(&y, 80);
    const f32 tap = jump_rise(&y, 3);
    const f32 ideal = TUNING_JUMP_SPEED * TUNING_JUMP_SPEED / (2.0f * TUNING_GRAVITY);
    CHECK(full > ideal - 6.0f && full < ideal + 1.0f); // ~50 px, discrete integration loses a bit
    CHECK(tap > 4.0f && tap < full * 0.5f);            // a tap is a short hop
    arena_free(&y.arena);
}

static void test_head_bump(void) {
    Yard y;
    CHECK(yard_init(&y));
    InputFrame in;
    // Under the slab (bottom edge at y = 144): the 50 px jump cannot complete.
    Companion c = settled(&y, 24.0f * TILE_SIZE + 8.0f, YARD_FLOOR, &in);
    bool bumped = false;
    for (int i = 0; i < 60; i++) {
        run_ticks(&c, &y.lv, &in, KEY(ACT_JUMP), 1);
        bumped = bumped || c.body.hit_ceiling;
        CHECK(c.body.pos.y >= 144.0f);
    }
    CHECK(bumped);
    arena_free(&y.arena);
}

static void test_coyote_time(void) {
    Yard y;
    CHECK(yard_init(&y));
    // Walk left off the left edge of the platform (x = 128), then press jump `delay` ticks later.
    const int delays[] = {0, 3, 4, 8, 12};
    const bool expect_jump[] = {true, true, true, false, false};
    for (usize k = 0; k < ARRAY_LEN(delays); k++) {
        InputFrame in;
        Companion c = settled(&y, 9.0f * TILE_SIZE, YARD_PLATFORM_TOP, &in);
        CHECK(c.body.on_ground);
        int guard = 0;
        while (c.body.on_ground && guard++ < 200) {
            run_ticks(&c, &y.lv, &in, KEY(ACT_LEFT), 1);
        }
        CHECK(!c.body.on_ground); // walked off the edge
        run_ticks(&c, &y.lv, &in, KEY(ACT_LEFT), delays[k]);
        run_ticks(&c, &y.lv, &in, KEY(ACT_LEFT) | KEY(ACT_JUMP), 1);
        bool jumped = c.body.vel.y < 0.0f;
        if (jumped != expect_jump[k]) {
            fprintf(stderr, "coyote delay %d: jumped=%d expected=%d\n", delays[k], (int)jumped,
                    (int)expect_jump[k]);
            g_failures++;
        }
    }
    arena_free(&y.arena);
}

static void test_jump_buffer(void) {
    Yard y;
    CHECK(yard_init(&y));
    InputFrame in;
    // Start 24 px above the floor (lands at tick 13) and press jump at tick 10, still falling.
    // The buffer lasts 6 ticks including the press tick, so a press 4 ticks early must be honoured.
    Companion c;
    companion_init(&c, vec2(OPEN_X, YARD_FLOOR - 24.0f));
    memset(&in, 0, sizeof(in));
    int landed = -1, took_off = -1;
    for (int i = 0; i < 40; i++) {
        input_frame_update(&in, i >= 10 ? KEY(ACT_JUMP) : 0);
        companion_update(&c, &y.lv, &in);
        if (landed < 0 && c.body.on_ground) {
            landed = i;
        }
        if (landed >= 0 && took_off < 0 && c.body.vel.y < 0.0f) {
            took_off = i;
        }
    }
    CHECK(landed > 10);                 // really was in the air when jump was pressed
    CHECK(took_off >= 0);               // and the buffered press produced a jump
    CHECK(took_off - landed <= 1);      // right on landing
    arena_free(&y.arena);
}

static void test_drop_through_platform(void) {
    Yard y;
    CHECK(yard_init(&y));
    InputFrame in;
    const f32 x = 10.0f * TILE_SIZE;

    // Idle and down alone: stays on the platform.
    Companion a = settled(&y, x, YARD_PLATFORM_TOP, &in);
    run_ticks(&a, &y.lv, &in, KEY(ACT_DOWN), 40);
    CHECK(a.body.on_ground);
    CHECK_NEAR(a.body.pos.y, YARD_PLATFORM_TOP - TUNING_COMPANION_H, 0.0);

    // Down + jump: falls to the floor instead of jumping.
    Companion b = settled(&y, x, YARD_PLATFORM_TOP, &in);
    run_ticks(&b, &y.lv, &in, KEY(ACT_DOWN) | KEY(ACT_JUMP), 1);
    CHECK(b.body.vel.y > 0.0f);
    run_ticks(&b, &y.lv, &in, 0, 60);
    CHECK(b.body.on_ground);
    CHECK_NEAR(b.body.pos.y, YARD_FLOOR - TUNING_COMPANION_H, 0.0);

    // Plain jump on the platform: goes up, not down.
    Companion c = settled(&y, x, YARD_PLATFORM_TOP, &in);
    run_ticks(&c, &y.lv, &in, KEY(ACT_JUMP), 1);
    CHECK(c.body.vel.y < 0.0f);

    // Down + jump on solid floor: a normal jump (nothing to drop through).
    Companion d = settled(&y, OPEN_X, YARD_FLOOR, &in);
    run_ticks(&d, &y.lv, &in, KEY(ACT_DOWN) | KEY(ACT_JUMP), 1);
    CHECK(d.body.vel.y < 0.0f);
    arena_free(&y.arena);
}

static void test_jump_up_through_platform(void) {
    Yard y;
    CHECK(yard_init(&y));
    InputFrame in;
    // Jump from the floor right under the platform (top is 32 px above the floor).
    Companion c = settled(&y, 10.0f * TILE_SIZE, YARD_FLOOR, &in);
    f32 min_y = c.body.pos.y;
    for (int i = 0; i < 120; i++) {
        run_ticks(&c, &y.lv, &in, i < 40 ? KEY(ACT_JUMP) : 0, 1);
        min_y = MIN(min_y, c.body.pos.y);
    }
    CHECK(min_y < YARD_PLATFORM_TOP - TUNING_COMPANION_H); // got above the platform
    CHECK(c.body.on_ground);
    CHECK_NEAR(c.body.pos.y, YARD_PLATFORM_TOP - TUNING_COMPANION_H, 0.0); // and stands on it
    arena_free(&y.arena);
}

static void test_ladder_climb_to_top(void) {
    Yard y;
    CHECK(yard_init(&y));
    InputFrame in;
    Companion c = settled(&y, LADDER_X - 3.0f, YARD_FLOOR, &in); // slightly off-center
    run_ticks(&c, &y.lv, &in, KEY(ACT_UP), 10);
    CHECK(c.on_ladder);
    CHECK_NEAR(c.body.pos.x + TUNING_COMPANION_W * 0.5f, LADDER_X, 1e-3); // snapped to the column
    CHECK_NEAR(c.body.vel.y, -TUNING_CLIMB_SPEED, 1e-3);

    // Idle on the ladder: no gravity.
    const f32 y_before = c.body.pos.y;
    run_ticks(&c, &y.lv, &in, 0, 30);
    CHECK(c.on_ladder);
    CHECK_NEAR(c.body.pos.y, y_before, 0.0);

    // Keep climbing: ends standing on the slab-level floor, off the ladder.
    run_ticks(&c, &y.lv, &in, KEY(ACT_UP), 120);
    CHECK(!c.on_ladder);
    CHECK(c.body.on_ground);
    CHECK_NEAR(c.body.pos.y, YARD_SLAB_TOP - TUNING_COMPANION_H, 0.0);
    // And does not immediately re-grab while still holding up.
    run_ticks(&c, &y.lv, &in, KEY(ACT_UP), 30);
    CHECK(!c.on_ladder);
    CHECK_NEAR(c.body.pos.y, YARD_SLAB_TOP - TUNING_COMPANION_H, 0.0);
    arena_free(&y.arena);
}

static void test_ladder_descend_from_top(void) {
    Yard y;
    CHECK(yard_init(&y));
    InputFrame in;
    Companion c = settled(&y, LADDER_X, YARD_SLAB_TOP, &in);
    CHECK(c.body.on_ground);
    CHECK(!c.on_ladder);
    run_ticks(&c, &y.lv, &in, KEY(ACT_DOWN), 5);
    CHECK(c.on_ladder); // grabbed through the one-way top
    run_ticks(&c, &y.lv, &in, KEY(ACT_DOWN), 120);
    CHECK(!c.on_ladder);
    CHECK(c.body.on_ground);
    CHECK_NEAR(c.body.pos.y, YARD_FLOOR - TUNING_COMPANION_H, 0.0);
    // Holding down at the foot must not re-grab (that would block walking).
    run_ticks(&c, &y.lv, &in, KEY(ACT_DOWN), 30);
    CHECK(!c.on_ladder);
    arena_free(&y.arena);
}

static void test_ladder_jump_off(void) {
    Yard y;
    CHECK(yard_init(&y));
    InputFrame in;
    Companion c = settled(&y, LADDER_X, YARD_FLOOR, &in);
    run_ticks(&c, &y.lv, &in, KEY(ACT_UP), 20);
    CHECK(c.on_ladder);

    // Jump + direction: hop off sideways.
    run_ticks(&c, &y.lv, &in, KEY(ACT_UP) | KEY(ACT_RIGHT) | KEY(ACT_JUMP), 1);
    CHECK(!c.on_ladder);
    CHECK(c.body.vel.x > 0.0f);
    CHECK(c.body.vel.y < 0.0f);
    // Up still held: no instant re-grab during the cooldown.
    run_ticks(&c, &y.lv, &in, KEY(ACT_UP) | KEY(ACT_RIGHT), 6);
    CHECK(!c.on_ladder);

    // Jump with no direction: just let go and drop.
    Companion d = settled(&y, LADDER_X, YARD_FLOOR, &in);
    run_ticks(&d, &y.lv, &in, KEY(ACT_UP), 20);
    CHECK(d.on_ladder);
    run_ticks(&d, &y.lv, &in, KEY(ACT_JUMP), 1);
    CHECK(!d.on_ladder);
    CHECK(d.body.vel.y >= 0.0f);
    arena_free(&y.arena);
}

static void test_walking_past_ladder_does_not_grab(void) {
    Yard y;
    CHECK(yard_init(&y));
    InputFrame in;
    Companion c = settled(&y, 18.0f * TILE_SIZE, YARD_FLOOR, &in);
    bool ever_grabbed = false;
    for (int i = 0; i < 90; i++) {
        run_ticks(&c, &y.lv, &in, KEY(ACT_RIGHT), 1);
        ever_grabbed = ever_grabbed || c.on_ladder;
    }
    CHECK(!ever_grabbed);
    CHECK(c.body.pos.x > 21.0f * TILE_SIZE); // went right past it
    arena_free(&y.arena);
}

int main(void) {
    RUN_TEST(test_walk_accel_and_cap);
    RUN_TEST(test_facing_follows_input);
    RUN_TEST(test_wall_blocks_walking);
    RUN_TEST(test_variable_jump_height);
    RUN_TEST(test_head_bump);
    RUN_TEST(test_coyote_time);
    RUN_TEST(test_jump_buffer);
    RUN_TEST(test_drop_through_platform);
    RUN_TEST(test_jump_up_through_platform);
    RUN_TEST(test_ladder_climb_to_top);
    RUN_TEST(test_ladder_descend_from_top);
    RUN_TEST(test_ladder_jump_off);
    RUN_TEST(test_walking_past_ladder_does_not_grab);
    TEST_MAIN_END();
}
