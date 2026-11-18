// Sprites: the data is well formed, every sprite has pixels, the atlases differ only where they should.
#include "test.h"

#include "render/sprites.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static u8 *build(const u8 team[3], const u8 dark[3], bool flash) {
    i32 w, h;
    sprites_atlas_size(&w, &h);
    u8 *rgba = (u8 *)malloc((usize)w * (usize)h * 4u);
    sprites_build_atlas(rgba, team, dark, flash);
    return rgba;
}

static u32 opaque_in(const u8 *rgba, Rect r, i32 stride) {
    u32 n = 0;
    for (i32 y = (i32)r.y; y < (i32)(r.y + r.h); y++) {
        for (i32 x = (i32)r.x; x < (i32)(r.x + r.w); x++) {
            n += rgba[(y * stride + x) * 4 + 3] ? 1u : 0u;
        }
    }
    return n;
}

static void test_the_sprite_data_is_valid(void) {
    char err[128] = "";
    const bool ok = sprites_validate(err, sizeof(err));
    if (!ok) {
        fprintf(stderr, "  %s\n", err);
    }
    CHECK(ok);
}

static void test_every_sprite_fits_its_cell_and_has_pixels(void) {
    const u8 red[3] = {230, 90, 70}, dark[3] = {150, 50, 40};
    u8 *atlas = build(red, dark, false);
    i32 w, h;
    sprites_atlas_size(&w, &h);
    for (i32 id = 0; id < SPR_COUNT; id++) {
        const Rect r = sprite_rect((SpriteId)id);
        const SpriteInfo info = sprite_info((SpriteId)id);
        const f32 span = (f32)(id >= SPR_BIG_FIRST ? 2 * SPRITE_CELL : SPRITE_CELL);
        CHECK((f32)info.w <= span && (f32)info.h <= span);
        CHECK(fmodf(r.x, (f32)SPRITE_CELL) == 0.0f && fmodf(r.y, (f32)SPRITE_CELL) == 0.0f);
        CHECK(r.x + r.w <= (f32)w && r.y + r.h <= (f32)h);
        CHECK(info.pivot.x >= 0.0f && info.pivot.x <= (f32)info.w && info.pivot.y >= 0.0f && info.pivot.y <= (f32)info.h);
        CHECK(opaque_in(atlas, r, w) > 8);
    }
    free(atlas);
}

static bool rects_overlap(Rect a, Rect b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static void test_no_two_sprites_share_atlas_pixels(void) {
    for (i32 a = 0; a < SPR_COUNT; a++) {
        for (i32 b = a + 1; b < SPR_COUNT; b++) {
            CHECK(!rects_overlap(sprite_rect((SpriteId)a), sprite_rect((SpriteId)b)));
        }
    }
}

// Terrain tiles cover their whole 16x16 square (no holes between tiles) and keep their colors in every team atlas;
// the base takes the team colors.
static void test_terrain_tiles_are_solid_and_team_free(void) {
    const u8 red[3] = {230, 90, 70}, red_dark[3] = {150, 50, 40}, blue[3] = {80, 150, 235}, blue_dark[3] = {40, 90, 160};
    u8 *a = build(red, red_dark, false), *b = build(blue, blue_dark, false);
    i32 w, h;
    sprites_atlas_size(&w, &h);
    static const SpriteId tiles[] = {SPR_TILE_GRASS0, SPR_TILE_GRASS1, SPR_TILE_GRASS2,
                                     SPR_TILE_DIRT0,  SPR_TILE_DIRT1,  SPR_TILE_DIRT2};
    for (usize k = 0; k < sizeof(tiles) / sizeof(tiles[0]); k++) {
        const Rect r = sprite_rect(tiles[k]);
        CHECK(r.w == 16.0f && r.h == 16.0f);
        CHECK(opaque_in(a, r, w) == 256u);
        CHECK(sprite_info(tiles[k]).pivot.x == 0.0f && sprite_info(tiles[k]).pivot.y == 0.0f);
    }
    bool terrain_differs = false, base_differs = false;
    const SpriteId terrain[] = {SPR_TILE_GRASS0, SPR_TILE_LADDER, SPR_TILE_PLANK, SPR_DECO_TUFT}, base = SPR_BASE;
    for (usize k = 0; k <= sizeof(terrain) / sizeof(terrain[0]); k++) {
        const bool is_base = k == sizeof(terrain) / sizeof(terrain[0]);
        const Rect r = sprite_rect(is_base ? base : terrain[k]);
        for (i32 y = (i32)r.y; y < (i32)(r.y + r.h); y++) {
            const usize i = (usize)(y * w + (i32)r.x) * 4u;
            const bool differs = memcmp(a + i, b + i, (usize)r.w * 4u) != 0;
            terrain_differs |= differs && !is_base;
            base_differs |= differs && is_base;
        }
    }
    CHECK(!terrain_differs && base_differs);
    free(a);
    free(b);
}

static void test_soldiers_stand_on_their_feet(void) {
    const u8 red[3] = {230, 90, 70}, dark[3] = {150, 50, 40};
    u8 *atlas = build(red, dark, false);
    i32 w, h;
    sprites_atlas_size(&w, &h);
    for (i32 v = 0; v < SPRITE_VARIANTS; v++) {
        for (i32 p = 0; p < POSE_COUNT; p++) {
            const Rect r = sprite_rect(sprite_body(v, (BodyPose)p));
            CHECK(r.w == SPRITE_BODY_W && r.h == SPRITE_BODY_H);
            // The head reaches the top rows; on the ground the boots reach the bottom row (in the air the legs tuck).
            const Rect top = {r.x, r.y, r.w, 2.0f}, bottom = {r.x, r.y + r.h - 2.0f, r.w, 2.0f};
            CHECK(opaque_in(atlas, top, w) > 0);
            if (p != POSE_JUMP) {
                CHECK(opaque_in(atlas, bottom, w) > 0);
            }
        }
    }
    CHECK(sprite_pose_bob(POSE_IDLE0) == 0 && sprite_pose_bob(POSE_COUNT) == 0);
    free(atlas);
}

static void test_team_colors_change_only_the_team_pixels(void) {
    const u8 red[3] = {230, 90, 70}, red_dark[3] = {150, 50, 40}, blue[3] = {80, 150, 235}, blue_dark[3] = {40, 90, 160};
    u8 *a = build(red, red_dark, false), *b = build(blue, blue_dark, false), *white = build(red, red_dark, true);
    i32 w, h;
    sprites_atlas_size(&w, &h);
    // A soldier changes color with the team, a crate does not.
    const Rect body = sprite_rect(sprite_body(0, POSE_IDLE0)), crate = sprite_rect(SPR_CRATE_RED);
    bool body_differs = false, crate_differs = false;
    for (i32 y = 0; y < (i32)body.h; y++) {
        for (i32 x = 0; x < (i32)body.w; x++) {
            const usize i = (usize)(((i32)body.y + y) * w + (i32)body.x + x) * 4u;
            body_differs |= memcmp(a + i, b + i, 4) != 0;
        }
    }
    for (i32 y = 0; y < (i32)crate.h; y++) {
        for (i32 x = 0; x < (i32)crate.w; x++) {
            const usize i = (usize)(((i32)crate.y + y) * w + (i32)crate.x + x) * 4u;
            crate_differs |= memcmp(a + i, b + i, 4) != 0;
        }
    }
    CHECK(body_differs && !crate_differs);
    // The flash atlas: same silhouettes, all white.
    bool white_ok = true;
    for (usize i = 0; i < (usize)w * (usize)h; i++) {
        white_ok &= (a[i * 4 + 3] != 0) == (white[i * 4 + 3] != 0);
        white_ok &= white[i * 4 + 3] == 0 || (white[i * 4] == 255 && white[i * 4 + 1] == 255 && white[i * 4 + 2] == 255);
    }
    CHECK(white_ok);
    // Red and blue crates differ.
    const Rect blue_crate = sprite_rect(SPR_CRATE_BLUE);
    CHECK(memcmp(a + (usize)((i32)crate.y * w + (i32)crate.x + 8 + 6 * w) * 4u,
                 a + (usize)((i32)blue_crate.y * w + (i32)blue_crate.x + 8 + 6 * w) * 4u, 3) != 0);
    free(a);
    free(b);
    free(white);
}

int main(void) {
    RUN_TEST(test_the_sprite_data_is_valid);
    RUN_TEST(test_every_sprite_fits_its_cell_and_has_pixels);
    RUN_TEST(test_no_two_sprites_share_atlas_pixels);
    RUN_TEST(test_terrain_tiles_are_solid_and_team_free);
    RUN_TEST(test_soldiers_stand_on_their_feet);
    RUN_TEST(test_team_colors_change_only_the_team_pixels);
    TEST_MAIN_END();
}
