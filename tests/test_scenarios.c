// Scripted runs on the real test arena: they prove the level can be traversed with the current
// tuning (jump height, speeds...). If you retune the physics and one of these breaks, either the
// level or the tuning needs an adjustment.
#include "test.h"

#include "app/sim.h"
#include "game/tuning.h"
#include "game/walker.h"
#include "test_helpers.h"

#ifndef CS_ASSET_DIR
#error "CS_ASSET_DIR must be defined by CMake"
#endif

static bool scenario_sim(Sim *sim, f32 x, f32 feet_y, const char *script) {
    SimConfig cfg = {.level_path = CS_ASSET_DIR "/levels/test_arena.lvl",
                     .seed = 1,
                     .script = script,
                     .has_start = true,
                     .start_feet = vec2(x, feet_y)};
    char err[256] = "";
    bool ok = sim_init(sim, &cfg, err, sizeof(err));
    if (!ok) {
        fprintf(stderr, "sim_init failed: %s\n", err);
    }
    return ok;
}

typedef struct Scenario {
    const char *name;
    f32 start_x, start_feet_y;
    const char *script;
    u32 at_tick;     // state is checked after this many ticks
    f32 feet_y;      // expected feet height (pixels)
    f32 min_x, max_x; // expected range of the body center
    bool on_ladder;
} Scenario;

// Level geometry (tile = 16 px): ground top y=432; thin platforms at y=400 (x 384..448),
// y=368 (x 464..528), y=336 (x 544..608); slab top y=320 from x=608 to 976; ladder column x=976..992
// from the slab down to the ground; water pit x 1024..1152 with a floating platform at y=400
// (x 1056..1120).
static const Scenario SCENARIOS[] = {
    {"ground -> thin platform 1", 360, 432, "R:12 RJ:30 R:20 .:20", 50, 400, 384, 448, false},
    {"platform 1 -> platform 2", 430, 400, "R:4 RJ:30 R:30 .:10", 45, 368, 464, 528, false},
    {"platform 2 -> platform 3", 520, 368, "R:2 RJ:30 R:30 .:10", 45, 336, 544, 608, false},
    {"platform 3 -> slab", 598, 336, "R:2 RJ:30 R:30 .:10", 45, 320, 608, 976, false},
    {"climb the ladder to the slab", 984, 432, "U:200", 150, 320, 983, 985, false},
    {"descend the ladder from the slab", 984, 320, "D:200", 150, 432, 983, 985, false},
    {"half way up the ladder", 984, 432, "U:40", 40, 432 - 40 * TUNING_CLIMB_SPEED / TICK_HZ, 983,
     985, true},
    {"ground -> floating platform over the pit", 1014, 432, "R:5 RJ:30 R:40 .:10", 40, 400, 1056,
     1120, false},
    {"floating platform -> far bank", 1112, 400, "R:2 RJ:30 R:50 .:10", 60, 432, 1152, 1280, false},
    {"drop through a thin platform", 416, 400, ".:3 DJ:1 .:60", 64, 432, 409, 423, false},
};

static void test_traversal_scenarios(void) {
    for (usize i = 0; i < ARRAY_LEN(SCENARIOS); i++) {
        const Scenario *s = &SCENARIOS[i];
        Sim sim;
        CHECK(scenario_sim(&sim, s->start_x, s->start_feet_y, s->script));
        for (u32 t = 0; t < s->at_tick; t++) {
            sim_tick(&sim, NULL);
        }
        const Companion *c = game_player(&sim.gs);
        const f32 feet = c->body.pos.y + c->body.size.y;
        const f32 cx = c->body.pos.x + c->body.size.x * 0.5f;
        bool ok = fabsf(feet - s->feet_y) < 0.6f && cx >= s->min_x && cx <= s->max_x &&
                  c->on_ladder == s->on_ladder && (s->on_ladder || c->body.on_ground);
        if (!ok) {
            fprintf(stderr,
                    "scenario '%s' after %u ticks: feet=%.2f (want %.2f) center_x=%.2f (want %.0f..%.0f) "
                    "ground=%d ladder=%d (want %d)\n",
                    s->name, (unsigned)s->at_tick, (double)feet, (double)s->feet_y, (double)cx,
                    (double)s->min_x, (double)s->max_x, (int)c->body.on_ground, (int)c->on_ladder,
                    (int)s->on_ladder);
            g_failures++;
        }
        CHECK_EQ_U64(sim.gs.respawns, 0);
        sim_free(&sim);
    }
}

static void test_water_respawn_on_real_level(void) {
    Sim sim;
    CHECK(scenario_sim(&sim, 1010, 432, "R:200")); // walks straight into the pit
    u64 respawn_tick = 0;
    for (int i = 0; i < 100 && respawn_tick == 0; i++) {
        sim_tick(&sim, NULL);
        if (sim.gs.respawns > 0) {
            respawn_tick = sim.gs.tick;
        }
    }
    CHECK(respawn_tick > 0 && respawn_tick < 60);
    const Body *b = &game_player(&sim.gs)->body;
    CHECK_NEAR(b->pos.x + b->size.x * 0.5f, sim.level.spawns[0].x, 1e-3);
    CHECK_NEAR(b->pos.y + b->size.y, sim.level.spawns[0].y, 1e-3);
    sim_free(&sim);
}

static void test_ceiling_over_the_stairs(void) {
    // Standing on the stair top (y=368) under a ceiling block whose underside is at y=336:
    // 32 px of room for a 22 px body, so a full jump (50 px) must bump the head.
    Sim sim;
    CHECK(scenario_sim(&sim, 248, 368, "J:20 .:40"));
    bool bumped = false;
    for (int i = 0; i < 60; i++) {
        sim_tick(&sim, NULL);
        bumped = bumped || game_player(&sim.gs)->body.hit_ceiling;
        CHECK(game_player(&sim.gs)->body.pos.y >= 336.0f);
    }
    CHECK(bumped);
    sim_free(&sim);
}

// ---- Walker bot -------------------------------------------------------------------------------

// Runs the walker for up to `max_ticks`; returns true when it arrived.
static bool walk(Sim *sim, f32 target_x, int max_ticks) {
    Walker w;
    walker_init(&w, target_x);
    InputFrame in = {0};
    for (int i = 0; i < max_ticks; i++) {
        walker_think(&w, &sim->level, game_player(&sim->gs), &in);
        sim_tick(sim, &in);
        if (walker_arrived(&w, game_player(&sim->gs)) && game_player(&sim->gs)->body.on_ground) {
            return true;
        }
    }
    return false;
}

static void test_walker_climbs_the_stairs(void) {
    Sim sim;
    SimConfig cfg = {.level_path = CS_ASSET_DIR "/levels/test_arena.lvl", .seed = 1};
    char err[128];
    CHECK(sim_init(&sim, &cfg, err, sizeof(err)));
    CHECK(walk(&sim, 250.0f, 600));
    const Body *b = &game_player(&sim.gs)->body;
    CHECK_NEAR(b->pos.y + b->size.y, 368.0, 0.01); // on top of the 4-tile stair block

    CHECK(walk(&sim, 56.0f, 900)); // and back down to the spawn
    CHECK_NEAR(b->pos.y + b->size.y, 432.0, 0.01);
    sim_free(&sim);
}

static void test_walker_hops_the_low_wall(void) {
    Sim sim;
    CHECK(scenario_sim(&sim, 300, 432, NULL));
    CHECK(walk(&sim, 360.0f, 400)); // 2-tile wall at x=320..336
    CHECK_NEAR(game_player(&sim.gs)->body.pos.y + game_player(&sim.gs)->body.size.y, 432.0, 0.01);
    sim_free(&sim);
}

static void test_walker_reports_an_impassable_wall(void) {
    TestMap m;
    map_init(&m, 20, 10);
    map_rect(&m, 0, 8, 19, 9, '#');
    map_rect(&m, 10, 3, 10, 7, '#'); // 5 tiles high: more than a jump
    map_put(&m, 2, 7, '1');
    char text[MAX_LEVEL_W * 10 + 64];
    map_text(&m, text, sizeof(text));

    Sim sim;
    SimConfig cfg = {.level_text = text, .seed = 1};
    char err[128];
    CHECK(sim_init(&sim, &cfg, err, sizeof(err)));
    Walker w;
    walker_init(&w, 280.0f);
    InputFrame in = {0};
    for (int i = 0; i < 600; i++) {
        walker_think(&w, &sim.level, game_player(&sim.gs), &in);
        sim_tick(&sim, &in);
    }
    CHECK(walker_stuck(&w));
    CHECK(!walker_arrived(&w, game_player(&sim.gs)));
    CHECK(game_player(&sim.gs)->body.pos.x < 10.0f * TILE_SIZE); // never got through
    sim_free(&sim);
}

int main(void) {
    RUN_TEST(test_traversal_scenarios);
    RUN_TEST(test_water_respawn_on_real_level);
    RUN_TEST(test_ceiling_over_the_stairs);
    RUN_TEST(test_walker_climbs_the_stairs);
    RUN_TEST(test_walker_hops_the_low_wall);
    RUN_TEST(test_walker_reports_an_impassable_wall);
    TEST_MAIN_END();
}
