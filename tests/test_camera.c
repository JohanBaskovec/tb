#include "test.h"

#include "core/rng.h"
#include "render/camera.h"
#include "test_helpers.h"

#define VIEW_W 640.0f
#define VIEW_H 360.0f

static bool big_level(Level *lv, Arena *a) {
    TestMap m;
    map_init(&m, 80, 30); // 1280 x 480 px
    return arena_init(a, 1 << 16) && map_load(&m, lv, a);
}

static void test_snap_and_clamp(void) {
    Arena a;
    Level lv;
    CHECK(big_level(&lv, &a));
    Camera c;
    camera_init(&c, VIEW_W, VIEW_H);

    camera_follow(&c, vec2(10, 10), 1, &lv, true);
    CHECK_NEAR(c.pos.x, 0.0, 0.0); // clamped at the top-left corner
    CHECK_NEAR(c.pos.y, 0.0, 0.0);

    camera_follow(&c, vec2(1270, 470), 1, &lv, true);
    CHECK_NEAR(c.pos.x, 1280.0f - VIEW_W, 0.0);
    CHECK_NEAR(c.pos.y, 480.0f - VIEW_H, 0.0);

    camera_follow(&c, vec2(640, 240), 1, &lv, true);
    // Centered on the target, give or take the look-ahead (1 px per follow call so far).
    CHECK(fabsf(c.pos.x + VIEW_W * 0.5f - 640.0f) < 5.0f);
    CHECK_NEAR(c.prev_pos.x, c.pos.x, 0.0); // no interpolation glitch after a snap
    arena_free(&a);
}

static void test_level_smaller_than_view_is_centered(void) {
    TestMap m;
    map_init(&m, 20, 10); // 320 x 160 px
    Arena a;
    Level lv;
    CHECK(arena_init(&a, 1 << 16) && map_load(&m, &lv, &a));
    Camera c;
    camera_init(&c, VIEW_W, VIEW_H);
    camera_follow(&c, vec2(50, 50), 1, &lv, true);
    CHECK_NEAR(c.pos.x, (320.0f - VIEW_W) * 0.5f, 0.0);
    CHECK_NEAR(c.pos.y, (160.0f - VIEW_H) * 0.5f, 0.0);
    for (int i = 0; i < 100; i++) {
        camera_follow(&c, vec2(300, 150), -1, &lv, false);
    }
    CHECK_NEAR(c.pos.x, (320.0f - VIEW_W) * 0.5f, 0.0);
    arena_free(&a);
}

static void test_dead_zone_does_not_move(void) {
    Arena a;
    Level lv;
    CHECK(big_level(&lv, &a));
    Camera c;
    camera_init(&c, VIEW_W, VIEW_H);
    camera_follow(&c, vec2(640, 240), 1, &lv, true);
    const Vec2 start = c.pos;
    const Vec2 center = vec2(start.x + VIEW_W * 0.5f, start.y + VIEW_H * 0.5f);
    // Target slightly left of center with look-ahead to the right: always inside the dead zone.
    for (int i = 0; i < 200; i++) {
        camera_follow(&c, vec2(center.x - 10.0f, center.y + 8.0f), 1, &lv, false);
    }
    CHECK_NEAR(c.pos.x, start.x, 1e-3);
    CHECK_NEAR(c.pos.y, start.y, 1e-3);
    arena_free(&a);
}

static void test_follows_target_and_stays_in_bounds(void) {
    Arena a;
    Level lv;
    CHECK(big_level(&lv, &a));
    Camera c;
    camera_init(&c, VIEW_W, VIEW_H);
    camera_follow(&c, vec2(100, 240), 1, &lv, true);

    // Target runs far to the right: the camera catches up and never goes backwards.
    f32 last_x = c.pos.x;
    for (int i = 0; i < 400; i++) {
        camera_follow(&c, vec2(1100, 240), 1, &lv, false);
        CHECK(c.pos.x >= last_x - 1e-4f);
        last_x = c.pos.x;
    }
    CHECK_NEAR(c.pos.x, 1280.0f - VIEW_W, 1e-2); // reached the right border
    // Target is inside the view.
    CHECK(1100.0f > c.pos.x && 1100.0f < c.pos.x + VIEW_W);

    // Random targets, including far outside the level: the view always stays inside it.
    Rng rng;
    rng_seed(&rng, 5, 5);
    for (int i = 0; i < 5000; i++) {
        Vec2 t = vec2((f32)rng_range_i32(&rng, -500, 1800), (f32)rng_range_i32(&rng, -300, 800));
        camera_follow(&c, t, rng_below(&rng, 2) ? 1 : -1, &lv, rng_below(&rng, 50) == 0);
        CHECK(c.pos.x >= 0.0f && c.pos.x <= 1280.0f - VIEW_W);
        CHECK(c.pos.y >= 0.0f && c.pos.y <= 480.0f - VIEW_H);
    }
    arena_free(&a);
}

static void test_set_and_interpolate(void) {
    Arena a;
    Level lv;
    CHECK(big_level(&lv, &a));
    Camera c;
    camera_init(&c, VIEW_W, VIEW_H);
    camera_set(&c, vec2(-50, 9999), &lv);
    CHECK_NEAR(c.pos.x, 0.0, 0.0);
    CHECK_NEAR(c.pos.y, 480.0f - VIEW_H, 0.0);

    camera_set(&c, vec2(100, 50), &lv);
    camera_follow(&c, vec2(1200, 50), 1, &lv, false); // moves
    CHECK(c.pos.x > c.prev_pos.x);
    CHECK_NEAR(camera_interpolated(&c, 0.0f).x, c.prev_pos.x, 1e-6);
    CHECK_NEAR(camera_interpolated(&c, 1.0f).x, c.pos.x, 1e-6);
    Vec2 mid = camera_interpolated(&c, 0.5f);
    CHECK(mid.x > c.prev_pos.x && mid.x < c.pos.x);
    arena_free(&a);
}

int main(void) {
    RUN_TEST(test_snap_and_clamp);
    RUN_TEST(test_level_smaller_than_view_is_centered);
    RUN_TEST(test_dead_zone_does_not_move);
    RUN_TEST(test_follows_target_and_stays_in_bounds);
    RUN_TEST(test_set_and_interpolate);
    TEST_MAIN_END();
}
