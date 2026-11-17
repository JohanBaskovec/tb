#include "test.h"

#include "app/sim.h"
#include "game/tuning.h"
#include "test_helpers.h"

#include <stdlib.h>

#ifndef CS_ASSET_DIR
#error "CS_ASSET_DIR must be defined by CMake"
#endif

static bool real_sim(Sim *sim, u64 seed, bool bot, const char *script) {
    SimConfig cfg = {.level_path = CS_ASSET_DIR "/levels/test_arena.lvl",
                     .seed = seed,
                     .use_bot = bot,
                     .script = script};
    char err[256] = "";
    bool ok = sim_init(sim, &cfg, err, sizeof(err));
    if (!ok) {
        fprintf(stderr, "sim_init failed: %s\n", err);
    }
    return ok;
}

// ---- Invariants -----------------------------------------------------------------------------

static bool overlaps_solid(const Level *lv, const Body *b) {
    const f32 e = 0.01f;
    i32 tx0 = level_px_to_tile(b->pos.x + e), tx1 = level_px_to_tile(b->pos.x + b->size.x - e);
    i32 ty0 = level_px_to_tile(b->pos.y + e), ty1 = level_px_to_tile(b->pos.y + b->size.y - e);
    for (i32 ty = ty0; ty <= ty1; ty++) {
        for (i32 tx = tx0; tx <= tx1; tx++) {
            if (level_is_solid(lv, tx, ty)) {
                return true;
            }
        }
    }
    return false;
}

// Something solid or one-way directly under the feet. The shrink is smaller than the physics'
// own epsilon (0.001), so this never reports "no support" for a resting body the physics accepts.
static bool has_support(const Level *lv, const Body *b) {
    i32 ty = level_px_to_tile(b->pos.y + b->size.y + 0.5f);
    i32 tx0 = level_px_to_tile(b->pos.x + 0.0005f);
    i32 tx1 = level_px_to_tile(b->pos.x + b->size.x - 0.0005f);
    for (i32 tx = tx0; tx <= tx1; tx++) {
        if (level_is_solid(lv, tx, ty) || level_is_one_way(lv, tx, ty)) {
            return true;
        }
    }
    return false;
}

typedef struct Coverage {
    bool ladder, drop, jump, respawn, upper_slab;
    f32 min_y;
    f32 max_x;
} Coverage;

static void check_invariants(const Sim *sim, Coverage *cov) {
    const GameState *gs = &sim->gs;
    const Companion *c = &gs->player;
    const Body *b = &c->body;
    const Level *lv = &sim->level;

    if (overlaps_solid(lv, b)) {
        fprintf(stderr, "tick %llu: body inside a solid tile at (%.3f, %.3f)\n",
                (unsigned long long)gs->tick, (double)b->pos.x, (double)b->pos.y);
        g_failures++;
    }
    CHECK(b->pos.x >= 0.0f && b->pos.x + b->size.x <= level_width_px(lv));
    CHECK(b->pos.y + b->size.y <= level_height_px(lv) + 1.0f); // respawn triggers at the water
    CHECK(fabsf(b->vel.x) <= TUNING_WALK_SPEED + 0.01f);
    CHECK(b->vel.y >= -TUNING_JUMP_SPEED - 0.01f && b->vel.y <= TUNING_MAX_FALL_SPEED + 0.01f);
    if (b->on_ground && !has_support(lv, b)) {
        fprintf(stderr, "tick %llu: on_ground without support at pos=(%.4f, %.4f) ladder=%d\n",
                (unsigned long long)gs->tick, (double)b->pos.x, (double)b->pos.y, (int)c->on_ladder);
        g_failures++;
    }
    CHECK(c->facing == 1 || c->facing == -1);
    CHECK(sim->cam.pos.x >= 0.0f && sim->cam.pos.x <= level_width_px(lv) - LOGICAL_W);
    CHECK(sim->cam.pos.y >= 0.0f && sim->cam.pos.y <= level_height_px(lv) - LOGICAL_H);

    cov->ladder = cov->ladder || c->on_ladder;
    cov->drop = cov->drop || c->drop_ticks > 0;
    cov->jump = cov->jump || b->vel.y < -TUNING_JUMP_SPEED * 0.9f;
    cov->respawn = cov->respawn || gs->respawns > 0;
    cov->upper_slab = cov->upper_slab || (b->on_ground && b->pos.y + b->size.y <= 20.0f * TILE_SIZE);
    cov->min_y = MIN(cov->min_y, b->pos.y);
    cov->max_x = MAX(cov->max_x, b->pos.x);
}

static void test_bot_invariants_on_real_level(void) {
    Coverage cov = {.min_y = 1e9f};
    for (u64 seed = 1; seed <= 24; seed++) {
        Sim sim;
        CHECK(real_sim(&sim, seed, true, NULL));
        for (int i = 0; i < 8000; i++) {
            sim_tick(&sim, NULL);
            check_invariants(&sim, &cov);
        }
        CHECK_EQ_U64(sim.gs.tick, 8000);
        sim_free(&sim);
    }
    printf("       coverage: ladder=%d drop=%d jump=%d respawn=%d upper=%d min_y=%.0f max_x=%.0f\n",
           cov.ladder, cov.drop, cov.jump, cov.respawn, cov.upper_slab, (double)cov.min_y,
           (double)cov.max_x);
    // The random bots must actually exercise the mechanics, not just stand still.
    CHECK(cov.jump);
    CHECK(cov.ladder);
    CHECK(cov.drop);
    CHECK(cov.respawn);
    CHECK(cov.max_x > 600.0f);
}

// ---- Determinism ----------------------------------------------------------------------------

static u64 run_hash(u64 seed, bool bot, const char *script, u32 ticks) {
    Sim sim;
    CHECK(real_sim(&sim, seed, bot, script));
    for (u32 i = 0; i < ticks; i++) {
        sim_tick(&sim, NULL);
    }
    u64 h = game_hash(&sim.gs);
    sim_free(&sim);
    return h;
}

static void test_determinism(void) {
    CHECK_EQ_U64(run_hash(42, true, NULL, 20000), run_hash(42, true, NULL, 20000));
    CHECK(run_hash(42, true, NULL, 20000) != run_hash(43, true, NULL, 20000));
    const char *script = "R:60 RJ:12 .:20 L:40 UJ:30";
    CHECK_EQ_U64(run_hash(1, false, script, 400), run_hash(1, false, script, 400));
    CHECK(run_hash(1, false, script, 400) != run_hash(1, false, "L:60", 400));
}

// ---- Setup and rules ------------------------------------------------------------------------

static void test_init_requires_spawn(void) {
    Sim sim;
    char err[128] = "";
    SimConfig cfg = {.level_text = "---\n...\n###\n", .seed = 1};
    CHECK(!sim_init(&sim, &cfg, err, sizeof(err)));
    CHECK(strstr(err, "spawn") != NULL);

    cfg.level_text = "---\n.1.\n###\n";
    CHECK(sim_init(&sim, &cfg, err, sizeof(err)));
    sim_free(&sim);

    cfg.level_text = NULL;
    cfg.level_path = CS_ASSET_DIR "/levels/nope.lvl";
    CHECK(!sim_init(&sim, &cfg, err, sizeof(err)));

    cfg.level_text = "---\n.1.\n###\n";
    cfg.script = "Z:5";
    CHECK(!sim_init(&sim, &cfg, err, sizeof(err)));
    CHECK(strstr(err, "unknown key") != NULL);
}

static void test_player_settles_on_spawn(void) {
    Sim sim;
    CHECK(real_sim(&sim, 1, false, NULL));
    for (int i = 0; i < 10; i++) {
        sim_tick(&sim, NULL);
    }
    const Body *b = &sim.gs.player.body;
    CHECK(b->on_ground);
    CHECK_NEAR(b->pos.x + b->size.x * 0.5f, sim.level.spawns[0].x, 1e-3);
    CHECK_NEAR(b->pos.y + b->size.y, sim.level.spawns[0].y, 1e-3);
    CHECK_EQ_U64(sim.gs.respawns, 0);
    sim_free(&sim);
}

static void test_water_respawns_at_spawn(void) {
    TestMap m;
    map_init(&m, 24, 8);
    map_rect(&m, 0, 5, 23, 7, '#');
    map_rect(&m, 8, 5, 11, 7, '~'); // pit
    map_put(&m, 2, 4, '1');
    char text[MAX_LEVEL_W * 8 + 64];
    map_text(&m, text, sizeof(text));

    Sim sim;
    char err[128] = "";
    SimConfig cfg = {.level_text = text, .seed = 1, .script = "R:300"};
    CHECK(sim_init(&sim, &cfg, err, sizeof(err)));
    bool respawned = false;
    for (int i = 0; i < 300 && !respawned; i++) {
        sim_tick(&sim, NULL);
        respawned = sim.gs.respawns == 1;
    }
    CHECK(respawned);
    const Body *b = &sim.gs.player.body;
    CHECK_NEAR(b->pos.x + b->size.x * 0.5f, sim.level.spawns[0].x, 1e-3);
    CHECK_NEAR(b->pos.y + b->size.y, sim.level.spawns[0].y, 1e-3);
    CHECK_NEAR(b->vel.x, 0.0, 0.0);
    CHECK_NEAR(sim.cam.pos.x, sim.cam.prev_pos.x, 0.0); // camera snapped, no sweep across the map
    sim_free(&sim);
}

int main(void) {
    RUN_TEST(test_init_requires_spawn);
    RUN_TEST(test_player_settles_on_spawn);
    RUN_TEST(test_water_respawns_at_spawn);
    RUN_TEST(test_determinism);
    RUN_TEST(test_bot_invariants_on_real_level);
    TEST_MAIN_END();
}
