// Visual effects: particles from events, their physics, soldier poses, landing squash, screen shake.
#include "test.h"

#include "app/sim.h"
#include "game/combat.h"
#include "game/crate.h"
#include "test_helpers.h"

#include <string.h>

static Sim g_sim;
static FxList g_fx;

// Floor on rows 10-11, water at columns 14-16, spawn at column 2, a ladder at column 8 (rows 4-9).
static bool sim_on_test_map(void) {
    TestMap m;
    map_init(&m, 24, 12);
    map_rect(&m, 0, 10, 23, 11, '#');
    map_rect(&m, 14, 10, 16, 11, '~');
    map_rect(&m, 8, 4, 8, 9, 'H');
    map_put(&m, 2, 9, '1');
    char text[4096];
    map_text(&m, text, sizeof(text));
    const SimConfig cfg = {.level_text = text, .seed = 1};
    char err[256] = "";
    return sim_init(&g_sim, &cfg, err, sizeof(err));
}

static u32 count_kind(const FxList *fx, ParticleKind kind) {
    u32 n = 0;
    for (u32 i = 0; i < fx->part_count; i++) {
        n += fx->parts[i].kind == kind ? 1u : 0u;
    }
    return n;
}

static void consume_one(FxList *fx, EventType type, Vec2 pos, i32 a, i32 b, const GameState *gs) {
    EventQueue q = {0};
    events_push(&q, type, pos, a, b);
    fx_consume(fx, &q, gs);
}

static void test_an_explosion_throws_debris_smoke_and_sparks(void) {
    fx_init(&g_fx);
    g_fx.focus = vec2(100.0f, 100.0f);
    consume_one(&g_fx, EV_EXPLOSION, vec2(100.0f, 100.0f), 40, 0, NULL);
    CHECK(g_fx.count == 1 && g_fx.items[0].kind == FX_EXPLOSION);
    CHECK(count_kind(&g_fx, PART_CHUNK) >= 8);
    CHECK(count_kind(&g_fx, PART_SMOKE) >= 5);
    CHECK(count_kind(&g_fx, PART_SPARK) >= 5);
    CHECK(count_kind(&g_fx, PART_RING) == 1);
    CHECK(g_fx.shake > 2.0f); // right where the player looks
    // Far away: the same blast does not shake the screen.
    fx_init(&g_fx);
    g_fx.focus = vec2(2000.0f, 100.0f);
    consume_one(&g_fx, EV_EXPLOSION, vec2(100.0f, 100.0f), 40, 0, NULL);
    CHECK(g_fx.shake == 0.0f);
}

static void test_particles_fall_land_and_fade(void) {
    CHECK(sim_on_test_map());
    fx_init(&g_fx);
    consume_one(&g_fx, EV_EXPLOSION, vec2(5.0f * 16.0f, 9.0f * 16.0f), 40, 0, &g_sim.gs);
    for (int t = 0; t < 40; t++) {
        fx_step(&g_fx, &g_sim.level);
    }
    // Debris that came down rests on the floor (top of row 10), never inside it.
    u32 chunks = 0;
    for (u32 i = 0; i < g_fx.part_count; i++) {
        const Particle *p = &g_fx.parts[i];
        if (p->kind == PART_CHUNK) {
            chunks++;
            CHECK(!level_is_solid(&g_sim.level, level_px_to_tile(p->pos.x), level_px_to_tile(p->pos.y)));
        }
    }
    CHECK(chunks > 0);
    for (int t = 0; t < 200; t++) {
        fx_step(&g_fx, &g_sim.level);
    }
    CHECK(g_fx.part_count == 0 && g_fx.count == 0); // all gone
    CHECK(g_fx.shake == 0.0f);
    sim_free(&g_sim);
}

static void test_a_fallen_soldier_tumbles_away_in_its_colors(void) {
    CHECK(sim_on_test_map());
    fx_init(&g_fx);
    Soldier *s = &g_sim.gs.soldiers[0];
    s->variant = SOLDIER_HEAVY;
    s->c.facing = -1;
    consume_one(&g_fx, EV_SOLDIER_DIED, vec2(40.0f, 140.0f), 0, 0, &g_sim.gs);
    CHECK(count_kind(&g_fx, PART_CORPSE) == 1);
    const Particle *corpse = NULL;
    for (u32 i = 0; i < g_fx.part_count; i++) {
        corpse = g_fx.parts[i].kind == PART_CORPSE ? &g_fx.parts[i] : corpse;
    }
    CHECK(corpse && corpse->team == 0 && corpse->variant == SOLDIER_HEAVY && corpse->facing == -1);
    CHECK(corpse->vel.y < 0.0f && corpse->vel.x > 0.0f); // knocked up and backwards
    sim_free(&g_sim);
}

static void test_a_crate_lost_in_water_splashes_and_one_broken_on_land_splinters(void) {
    CHECK(sim_on_test_map());
    fx_init(&g_fx);
    consume_one(&g_fx, EV_CRATE_DESTROYED, vec2(15.0f * 16.0f, 9.5f * 16.0f), 0, 0, &g_sim.gs);
    CHECK(count_kind(&g_fx, PART_DOT) >= 8 && count_kind(&g_fx, PART_CHUNK) == 0);
    fx_init(&g_fx);
    consume_one(&g_fx, EV_CRATE_DESTROYED, vec2(5.0f * 16.0f, 9.0f * 16.0f), 0, 0, &g_sim.gs);
    CHECK(count_kind(&g_fx, PART_CHUNK) >= 6 && count_kind(&g_fx, PART_DOT) == 0);
    sim_free(&g_sim);
}

static void test_shots_eject_casings_and_rockets_leave_smoke(void) {
    fx_init(&g_fx);
    consume_one(&g_fx, EV_SHOT, vec2(50.0f, 50.0f), WEAPON_PISTOL, 1, NULL);
    CHECK(g_fx.count == 1 && g_fx.items[0].kind == FX_MUZZLE);
    CHECK(count_kind(&g_fx, PART_CHUNK) == 1 && g_fx.parts[0].vel.x < 0.0f); // thrown backwards
    CHECK(sim_on_test_map());
    fx_init(&g_fx);
    projectile_spawn(&g_sim.gs, WEAPON_ROCKET_LAUNCHER, 0, vec2(60.0f, 120.0f), vec2(200.0f, 0.0f));
    for (int t = 0; t < 5; t++) {
        fx_observe(&g_fx, &g_sim.gs);
    }
    CHECK(count_kind(&g_fx, PART_SMOKE) == 5); // one puff per tick behind the rocket
    sim_free(&g_sim);
}

static void tick(InputFrame *in, u32 mask) {
    input_frame_update(in, mask);
    sim_tick(&g_sim, in);
}

static void test_poses_follow_what_the_soldier_does(void) {
    CHECK(sim_on_test_map());
    InputFrame in = {0};
    for (int t = 0; t < 20; t++) {
        tick(&in, 0);
    }
    const GameState *gs = &g_sim.gs;
    const i32 me = gs->controlled;
    const BodyPose idle = fx_pose(&g_sim.fx, gs, me);
    CHECK(idle == POSE_IDLE0 || idle == POSE_IDLE1);
    bool seen[POSE_COUNT] = {false};
    for (int t = 0; t < 40; t++) { // running cycles through the four run poses
        tick(&in, KEY(ACT_RIGHT));
        seen[fx_pose(&g_sim.fx, gs, me)] = true;
    }
    CHECK(seen[POSE_RUN0] && seen[POSE_RUN1] && seen[POSE_RUN2] && seen[POSE_RUN3]);
    tick(&in, KEY(ACT_JUMP));
    tick(&in, KEY(ACT_JUMP));
    CHECK(fx_pose(&g_sim.fx, gs, me) == POSE_JUMP);
    bool fell = false, squashed = false;
    for (int t = 0; t < 60; t++) {
        tick(&in, KEY(ACT_JUMP));
        fell |= fx_pose(&g_sim.fx, gs, me) == POSE_FALL;
        squashed |= fx_squash(&g_sim.fx, me) > 0.5f;
    }
    CHECK(fell && squashed);
    for (int t = 0; t < 10; t++) {
        tick(&in, 0);
    }
    CHECK(fx_squash(&g_sim.fx, me) == 0.0f); // the squash is brief
    sim_free(&g_sim);
}

static void test_the_particle_pool_never_overflows(void) {
    fx_init(&g_fx);
    for (int k = 0; k < 100; k++) {
        consume_one(&g_fx, EV_BASE_DESTROYED, vec2(0.0f, 0.0f), 1, 0, NULL);
    }
    CHECK(g_fx.part_count == MAX_PARTICLES);
    CHECK(g_fx.count <= MAX_FX);
    const Vec2 shake = fx_shake_offset(&g_fx);
    CHECK(fabsf(shake.x) <= 7.0f && fabsf(shake.y) <= 7.0f);
}

int main(void) {
    RUN_TEST(test_an_explosion_throws_debris_smoke_and_sparks);
    RUN_TEST(test_particles_fall_land_and_fade);
    RUN_TEST(test_a_fallen_soldier_tumbles_away_in_its_colors);
    RUN_TEST(test_a_crate_lost_in_water_splashes_and_one_broken_on_land_splinters);
    RUN_TEST(test_shots_eject_casings_and_rockets_leave_smoke);
    RUN_TEST(test_poses_follow_what_the_soldier_does);
    RUN_TEST(test_the_particle_pool_never_overflows);
    TEST_MAIN_END();
}
