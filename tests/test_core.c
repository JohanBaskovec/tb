#include "test.h"

#include "core/arena.h"
#include "core/geom.h"
#include "core/input_frame.h"
#include "core/rng.h"

#include <stdint.h>

static void test_geom_scalars(void) {
    CHECK_NEAR(clampf(5.0f, 0.0f, 3.0f), 3.0f, 0.0);
    CHECK_NEAR(clampf(-1.0f, 0.0f, 3.0f), 0.0f, 0.0);
    CHECK_NEAR(clampf(2.0f, 0.0f, 3.0f), 2.0f, 0.0);
    CHECK_NEAR(lerpf(10.0f, 20.0f, 0.25f), 12.5f, 1e-6);
    CHECK_NEAR(approachf(0.0f, 10.0f, 3.0f), 3.0f, 0.0);
    CHECK_NEAR(approachf(9.0f, 10.0f, 3.0f), 10.0f, 0.0);
    CHECK_NEAR(approachf(10.0f, 0.0f, 4.0f), 6.0f, 0.0);
    CHECK_NEAR(approachf(1.0f, 0.0f, 4.0f), 0.0f, 0.0);
}

static void test_geom_rects(void) {
    Rect a = {0, 0, 10, 10};
    CHECK(rect_overlap(a, (Rect){5, 5, 10, 10}));
    CHECK(!rect_overlap(a, (Rect){10, 0, 5, 5})); // touching edges do not overlap
    CHECK(!rect_overlap(a, (Rect){20, 20, 1, 1}));
    CHECK(rect_contains(a, vec2(0, 0)));
    CHECK(!rect_contains(a, vec2(10, 5)));
    Vec2 v = vec2_add(vec2(1, 2), vec2_scale(vec2(3, 4), 2.0f));
    CHECK_NEAR(v.x, 7.0f, 0.0);
    CHECK_NEAR(v.y, 10.0f, 0.0);
}

static void test_arena(void) {
    Arena a;
    CHECK(arena_init(&a, 256));
    u8 *p = (u8 *)arena_push(&a, 3, 1);
    CHECK(p != NULL);
    u64 *q = ARENA_PUSH_STRUCT(&a, u64);
    CHECK(q != NULL);
    CHECK(((uintptr_t)q % _Alignof(u64)) == 0);
    CHECK_EQ_U64(*q, 0); // memory is zeroed
    CHECK(arena_push(&a, 1000, 1) == NULL);       // too big
    CHECK(arena_push(&a, (usize)-1, 8) == NULL);  // overflow-safe
    arena_reset(&a);
    CHECK_EQ_U64(a.used, 0);
    CHECK(arena_push(&a, 256, 1) != NULL); // exactly full
    CHECK(arena_push(&a, 1, 1) == NULL);
    arena_free(&a);
}

static void test_rng_reference_vector(void) {
    // Reference output of the official pcg32 demo: srandom(42, 54).
    Rng r;
    rng_seed(&r, 42u, 54u);
    static const u32 expected[] = {0xa15c02b7u, 0x7b47f409u, 0xba1d3330u,
                                   0x83d2f293u, 0xbfa4784bu, 0xcbed606eu};
    for (usize i = 0; i < ARRAY_LEN(expected); i++) {
        CHECK_EQ_U64(rng_next_u32(&r), expected[i]);
    }
}

static void test_rng_ranges(void) {
    Rng r;
    rng_seed(&r, 123, 1);
    u32 seen[6] = {0};
    for (int i = 0; i < 6000; i++) {
        i32 v = rng_range_i32(&r, -2, 3);
        CHECK(v >= -2 && v <= 3);
        seen[v + 2]++;
        f32 f = rng_f32(&r);
        CHECK(f >= 0.0f && f < 1.0f);
    }
    for (int i = 0; i < 6; i++) {
        CHECK(seen[i] > 800 && seen[i] < 1200); // roughly uniform (expected 1000)
    }
    // Same seed -> same stream.
    Rng a, b;
    rng_seed(&a, 9, 9);
    rng_seed(&b, 9, 9);
    for (int i = 0; i < 100; i++) {
        CHECK_EQ_U64(rng_next_u32(&a), rng_next_u32(&b));
    }
}

static void test_input_frame_edges(void) {
    InputFrame f = {0};
    input_frame_update(&f, action_bit(ACT_LEFT));
    CHECK(input_down(&f, ACT_LEFT) && input_pressed(&f, ACT_LEFT));
    CHECK(!input_released(&f, ACT_LEFT));
    input_frame_update(&f, action_bit(ACT_LEFT)); // still held
    CHECK(input_down(&f, ACT_LEFT) && !input_pressed(&f, ACT_LEFT));
    input_frame_update(&f, action_bit(ACT_RIGHT));
    CHECK(input_released(&f, ACT_LEFT) && input_pressed(&f, ACT_RIGHT));
    CHECK_NEAR(input_axis_x(&f), 1.0f, 0.0);
    input_frame_update(&f, action_bit(ACT_LEFT) | action_bit(ACT_RIGHT));
    CHECK_NEAR(input_axis_x(&f), 0.0f, 0.0); // opposite directions cancel
    input_frame_update(&f, action_bit(ACT_UP));
    CHECK_NEAR(input_axis_y(&f), -1.0f, 0.0);
}

int main(void) {
    RUN_TEST(test_geom_scalars);
    RUN_TEST(test_geom_rects);
    RUN_TEST(test_arena);
    RUN_TEST(test_rng_reference_vector);
    RUN_TEST(test_rng_ranges);
    RUN_TEST(test_input_frame_edges);
    TEST_MAIN_END();
}
