// Game -> presentation events: the queue itself, and the events the crate code raises.
#include "test.h"

#include "app/sim.h"
#include "game/combat.h"
#include "game/crate.h"
#include "test_helpers.h"

static void test_queue_basics(void) {
    EventQueue q = {0};
    CHECK_EQ_U64(q.count, 0);
    events_push(&q, EV_CRATE_SPAWNED, vec2(1, 2), 3, 4);
    events_push(&q, EV_PAD_BUILT, vec2(5, 6), 7, 8);
    events_push(&q, EV_CRATE_SPAWNED, vec2(0, 0), 0, 0);
    CHECK_EQ_U64(q.count, 3);
    CHECK(q.items[0].type == EV_CRATE_SPAWNED && q.items[0].a == 3 && q.items[0].b == 4);
    CHECK_NEAR(q.items[1].pos.x, 5.0, 0.0);
    CHECK_EQ_U64(events_count_of(&q, EV_CRATE_SPAWNED), 2);
    CHECK_EQ_U64(events_count_of(&q, EV_PAD_BUILT), 1);
    CHECK_EQ_U64(events_count_of(&q, EV_CRATE_THROWN), 0);
    events_clear(&q);
    CHECK_EQ_U64(q.count, 0);
}

static void test_queue_overflow_is_counted_not_fatal(void) {
    EventQueue q = {0};
    for (int i = 0; i < MAX_EVENTS + 10; i++) {
        events_push(&q, EV_CRATE_SETTLED, vec2(0, 0), i, 0);
    }
    CHECK_EQ_U64(q.count, MAX_EVENTS);
    CHECK_EQ_U64(q.dropped, 10);
    CHECK(q.items[MAX_EVENTS - 1].a == MAX_EVENTS - 1); // the oldest are kept
    events_clear(&q);
    CHECK_EQ_U64(q.dropped, 0);
}

typedef struct Env {
    Sim sim;
    InputFrame in;
    u32 seen[EV_TYPE_COUNT]; // events accumulated over all ticks
} Env;

// Floor on rows 10-11, water pit at columns 14-16, spawn at column 1, one drop zone above column 6.
static bool env_init(Env *e) {
    TestMap m;
    map_init(&m, 20, 12);
    map_rect(&m, 0, 10, 19, 11, '#');
    map_rect(&m, 14, 10, 16, 11, '~');
    map_put(&m, 1, 9, '1');
    map_put(&m, 6, 0, 'D');
    char text[4096] = "drop.1=red,1,1\n";
    map_text(&m, text + strlen(text), sizeof(text) - strlen(text));
    memset(e, 0, sizeof(*e));
    SimConfig cfg = {.level_text = text, .seed = 1};
    char err[256] = "";
    return sim_init(&e->sim, &cfg, err, sizeof(err));
}

static void tick(Env *e, u32 mask) {
    input_frame_update(&e->in, mask);
    sim_tick(&e->sim, &e->in);
    for (u32 i = 0; i < e->sim.gs.events.count; i++) {
        e->seen[e->sim.gs.events.items[i].type]++;
    }
}

static void ticks(Env *e, u32 mask, int n) {
    for (int i = 0; i < n; i++) {
        tick(e, mask);
    }
}

static void test_drop_and_landing_events(void) {
    Env e;
    CHECK(env_init(&e));
    ticks(&e, 0, 60 * 6);
    CHECK(e.seen[EV_CRATE_SPAWNED] >= 1);
    CHECK(e.seen[EV_CRATE_SETTLED] >= 1);
    CHECK(e.seen[EV_CRATE_SETTLED] <= e.seen[EV_CRATE_SPAWNED]);
    CHECK_EQ_U64(e.seen[EV_CRATE_SPAWNED], e.sim.gs.crate_stats.spawned);
    CHECK_EQ_U64(e.seen[EV_CRATE_SETTLED], e.sim.gs.crate_stats.settled);
    sim_free(&e.sim);
}

static void test_queue_holds_one_tick_only(void) {
    Env e;
    CHECK(env_init(&e));
    crate_spawn(&e.sim.gs, CRATE_BLUE, vec2(8 * 16, 9 * 16), CRATE_RESTING, -1);
    CHECK(e.sim.gs.events.count >= 1); // raised outside a tick: visible until the next one
    tick(&e, 0);
    CHECK_EQ_U64(e.sim.gs.events.count, 0); // cleared at the start of the tick, nothing happened
    sim_free(&e.sim);
}

static void test_carry_events_and_payload(void) {
    Env e;
    CHECK(env_init(&e));
    // A blue crate right in front of the soldier (he stands at column 1 facing right).
    crate_spawn(&e.sim.gs, CRATE_BLUE, vec2(2 * 16, 9 * 16), CRATE_RESTING, -1);
    ticks(&e, 0, 10);
    memset(e.seen, 0, sizeof(e.seen));

    tick(&e, KEY(ACT_GRAB));
    CHECK_EQ_U64(e.seen[EV_CRATE_PICKED_UP], 1);
    CHECK_EQ_U64(events_count_of(&e.sim.gs.events, EV_CRATE_PICKED_UP), 1);
    for (u32 i = 0; i < e.sim.gs.events.count; i++) {
        if (e.sim.gs.events.items[i].type == EV_CRATE_PICKED_UP) {
            CHECK_EQ_U64(e.sim.gs.events.items[i].a, CRATE_BLUE);
        }
    }
    tick(&e, 0);
    tick(&e, KEY(ACT_GRAB)); // put it back down
    CHECK_EQ_U64(e.seen[EV_CRATE_PLACED], 1);
    ticks(&e, 0, 30); // let it fall and settle
    CHECK(e.seen[EV_CRATE_SETTLED] >= 1);

    tick(&e, KEY(ACT_GRAB)); // pick up again
    tick(&e, 0);
    tick(&e, KEY(ACT_FIRE)); // and throw it
    CHECK_EQ_U64(e.seen[EV_CRATE_THROWN], 1);
    sim_free(&e.sim);
}

static void test_destroyed_in_water_and_respawn_events(void) {
    Env e;
    CHECK(env_init(&e));
    crate_spawn(&e.sim.gs, CRATE_RED, vec2(15 * 16, 8 * 16), CRATE_FALLING, -1);
    ticks(&e, 0, 60);
    CHECK_EQ_U64(e.seen[EV_CRATE_DESTROYED], 1);

    // The soldier walks into the water: respawn event.
    ticks(&e, KEY(ACT_RIGHT), 60 * 4);
    CHECK(e.seen[EV_PLAYER_RESPAWNED] >= 1);
    CHECK_EQ_U64(e.seen[EV_PLAYER_RESPAWNED], e.sim.gs.respawns);
    sim_free(&e.sim);
}

static void test_jump_and_landing_events(void) {
    Env e;
    CHECK(env_init(&e));
    ticks(&e, 0, 30); // standing still: no event
    memset(e.seen, 0, sizeof(e.seen));
    ticks(&e, 0, 10);
    CHECK_EQ_U64(e.seen[EV_JUMP] + e.seen[EV_LANDED], 0);
    tick(&e, KEY(ACT_JUMP));
    CHECK_EQ_U64(e.seen[EV_JUMP], 1);
    ticks(&e, KEY(ACT_JUMP), 20); // held: a full jump, still one event
    ticks(&e, 0, 60);
    CHECK_EQ_U64(e.seen[EV_JUMP], 1);
    CHECK_EQ_U64(e.seen[EV_LANDED], 1);
    // Walking along the floor never "lands".
    memset(e.seen, 0, sizeof(e.seen));
    ticks(&e, KEY(ACT_RIGHT), 30);
    CHECK_EQ_U64(e.seen[EV_LANDED], 0);
    sim_free(&e.sim);
}

static void test_events_do_not_change_the_hash(void) {
    // Same simulation, but one run drains/reads the queue and the other pokes at it: same state hash.
    u64 h[2];
    for (int run = 0; run < 2; run++) {
        Env e;
        CHECK(env_init(&e));
        for (int i = 0; i < 600; i++) {
            tick(&e, 0);
            if (run == 1) {
                events_push(&e.sim.gs.events, EV_PAD_BUILT, vec2(0, 0), 0, 0); // presentation noise
            }
        }
        h[run] = game_hash(&e.sim.gs);
        sim_free(&e.sim);
    }
    CHECK(h[0] == h[1]);
}

static void test_fx_follow_the_events(void) {
    EventQueue q = {0};
    static FxList fx;
    fx_init(&fx);
    events_push(&q, EV_EXPLOSION, vec2(10, 20), 40, WEAPON_GRENADES);
    events_push(&q, EV_SHOT, vec2(1, 2), WEAPON_PISTOL, 1);
    events_push(&q, EV_DAMAGE, vec2(3, 4), 12, 0);
    events_push(&q, EV_CRATE_SPAWNED, vec2(0, 0), 0, 0); // no effect for this one
    fx_consume(&fx, &q, NULL);
    CHECK_EQ_U64(fx.count, 3);
    CHECK(fx.items[0].kind == FX_EXPLOSION && fx.items[0].radius == 40.0f);
    CHECK(fx.items[1].kind == FX_MUZZLE && fx.items[2].kind == FX_SPARK);
    // The muzzle flash is the shortest: it goes first, the explosion lasts longest.
    for (int i = 0; i < 3; i++) {
        fx_step(&fx, NULL);
    }
    CHECK_EQ_U64(fx.count, 2);
    for (int i = 0; i < 5; i++) {
        fx_step(&fx, NULL);
    }
    CHECK_EQ_U64(fx.count, 1);
    CHECK(fx.items[0].kind == FX_EXPLOSION && fx_progress(&fx.items[0]) > 0.0f && fx_progress(&fx.items[0]) < 1.0f);
    for (int i = 0; i < 30; i++) {
        fx_step(&fx, NULL);
    }
    CHECK_EQ_U64(fx.count, 0);
    // A full list drops new effects instead of overflowing.
    for (u32 i = 0; i < MAX_FX + 10; i++) {
        events_push(&q, EV_EXPLOSION, vec2(0, 0), 8, 0);
        q.count = 1;
        fx_consume(&fx, &q, NULL);
    }
    CHECK_EQ_U64(fx.count, MAX_FX);
}

static void test_sim_drains_events_into_fx_every_tick(void) {
    TestMap m;
    map_init(&m, 20, 12);
    map_rect(&m, 0, 10, 19, 11, '#');
    map_put(&m, 1, 9, '1');
    char text[4096];
    map_text(&m, text, sizeof(text));
    Sim sim;
    SimConfig cfg = {.level_text = text, .seed = 1};
    char err[128];
    CHECK(sim_init(&sim, &cfg, err, sizeof(err)));
    InputFrame in = {0};
    input_frame_update(&in, 0);
    sim_tick(&sim, &in);
    combat_explode(&sim.gs, vec2(100, 100), WEAPON_GRENADES);
    sim_tick(&sim, &in); // the tick clears the queue first: the explosion raised before it is not seen
    CHECK_EQ_U64(sim.fx.count, 0);
    // An explosion raised during a tick (a grenade's fuse) shows up in the effects of that tick.
    combat_give_weapon(&sim.gs, WEAPON_GRENADES);
    input_frame_update(&in, action_bit(ACT_FIRE));
    sim_tick(&sim, &in);
    CHECK(sim.fx.count >= 1); // the muzzle flash of the throw
    sim_free(&sim);
}

int main(void) {
    RUN_TEST(test_queue_basics);
    RUN_TEST(test_queue_overflow_is_counted_not_fatal);
    RUN_TEST(test_drop_and_landing_events);
    RUN_TEST(test_queue_holds_one_tick_only);
    RUN_TEST(test_carry_events_and_payload);
    RUN_TEST(test_destroyed_in_water_and_respawn_events);
    RUN_TEST(test_jump_and_landing_events);
    RUN_TEST(test_events_do_not_change_the_hash);
    RUN_TEST(test_fx_follow_the_events);
    RUN_TEST(test_sim_drains_events_into_fx_every_tick);
    TEST_MAIN_END();
}
