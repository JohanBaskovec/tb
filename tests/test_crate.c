// Crates: drop zones, descent, stacking, solidity, picking up / carrying / placing / throwing.
// Everything runs through the real Sim (headless); levels are built in code.
#include "test.h"

#include "app/sim.h"
#include "game/combat.h"
#include "game/crate.h"
#include "game/tuning.h"
#include "game/walker.h"
#include "test_helpers.h"

#include <stdlib.h>

typedef struct Env {
    Sim sim;
    InputFrame in;
} Env;

// 20x12 map, floor rows 10-11 (top at y = 160), player spawn at column 1.
static TestMap base_map(void) {
    TestMap m;
    map_init(&m, 20, 12);
    map_rect(&m, 0, 10, 19, 11, '#');
    map_put(&m, 1, 9, '1');
    return m;
}

static bool env_init(Env *e, const TestMap *m, const char *header) {
    char text[8192];
    snprintf(text, sizeof(text), "%s", header ? header : "");
    map_text(m, text + strlen(text), sizeof(text) - strlen(text));
    memset(e, 0, sizeof(*e));
    SimConfig cfg = {.level_text = text, .seed = 1};
    char err[256] = "";
    bool ok = sim_init(&e->sim, &cfg, err, sizeof(err));
    if (!ok) {
        fprintf(stderr, "sim_init failed: %s\n", err);
    }
    return ok;
}

static GameState *gs_of(Env *e) {
    return &e->sim.gs;
}

static void tick(Env *e, u32 mask) {
    input_frame_update(&e->in, mask);
    sim_tick(&e->sim, &e->in);
}

static void ticks(Env *e, u32 mask, int n) {
    for (int i = 0; i < n; i++) {
        tick(e, mask);
    }
}

// A key press: one tick down, one tick up (so the next press is a new edge).
static void press(Env *e, u32 mask) {
    tick(e, mask);
    tick(e, 0);
}

static void teleport(Env *e, f32 x, f32 feet_y) {
    companion_init(game_player(gs_of(e)), vec2(x, feet_y));
    ticks(e, 0, 2);
}

static EntityId spawn_resting(Env *e, CrateKind kind, i32 cx, i32 cy) {
    return crate_spawn(gs_of(e), kind, vec2((f32)(cx * TILE_SIZE), (f32)(cy * TILE_SIZE)),
                       CRATE_RESTING, -1);
}

static EntityId spawn_free(Env *e, CrateKind kind, f32 x, f32 y, CrateState state) {
    return crate_spawn(gs_of(e), kind, vec2(x, y), state, -1);
}

static const Entity *crate_of(Env *e, EntityId id) {
    return pool_get_const(&gs_of(e)->entities, id);
}

static const Entity *resting_at(Env *e, i32 cx, i32 cy) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *c = &gs_of(e)->entities.slots[i];
        if (c->type == ENT_CRATE && !c->pending_destroy && c->as.crate.state == CRATE_RESTING &&
            c->as.crate.cell_x == cx && c->as.crate.cell_y == cy) {
            return c;
        }
    }
    return NULL;
}

static bool is_resting_at(Env *e, i32 cx, i32 cy) {
    const Entity *c = resting_at(e, cx, cy);
    return c != NULL && level_dyn_solid(&e->sim.level, cx, cy) &&
           c->body.pos.x == (f32)(cx * TILE_SIZE) && c->body.pos.y == (f32)(cy * TILE_SIZE);
}

static u32 resting_count(Env *e) {
    u32 n = 0;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *c = &gs_of(e)->entities.slots[i];
        n += (c->type == ENT_CRATE && !c->pending_destroy && c->as.crate.state == CRATE_RESTING);
    }
    return n;
}

static f32 player_feet(Env *e) {
    return game_player(gs_of(e))->body.pos.y + game_player(gs_of(e))->body.size.y;
}

// ---- Descent, landing, stacking --------------------------------------------------------------

static void test_descent_is_slow_and_constant(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    EntityId id = spawn_free(&e, CRATE_RED, 96, 0, CRATE_DESCENDING);
    ticks(&e, 0, 10);
    CHECK_NEAR(crate_of(&e, id)->body.pos.y, 10.0, 1e-4); // 60 px/s = exactly 1 px per tick
    CHECK(crate_of(&e, id)->as.crate.state == CRATE_DESCENDING);
    CHECK_NEAR(crate_of(&e, id)->body.vel.y, TUNING_CRATE_DESCENT_SPEED, 0.0);

    int landed_at = -1;
    for (int i = 11; i < 400 && landed_at < 0; i++) {
        tick(&e, 0);
        if (crate_of(&e, id)->as.crate.state == CRATE_RESTING) {
            landed_at = i;
        }
    }
    CHECK(landed_at >= 143 && landed_at <= 146); // 144 px at 1 px/tick
    CHECK(is_resting_at(&e, 6, 9));
    CHECK_EQ_U64(gs_of(&e)->crate_stats.settled, 1);
    sim_free(&e.sim);
}

static void test_stacking_from_the_bottom_up(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    // Three crates in the same column, falling together a few pixels apart.
    spawn_free(&e, CRATE_RED, 96, 0, CRATE_DESCENDING);
    spawn_free(&e, CRATE_BLUE, 96, 20, CRATE_DESCENDING);
    spawn_free(&e, CRATE_RED, 96, 40, CRATE_DESCENDING);
    ticks(&e, 0, 220);
    CHECK(is_resting_at(&e, 6, 9));
    CHECK(is_resting_at(&e, 6, 8));
    CHECK(is_resting_at(&e, 6, 7));
    CHECK_EQ_U64(resting_count(&e), 3);
    CHECK(resting_at(&e, 6, 8)->as.crate.kind == CRATE_BLUE); // order of arrival kept
    CHECK_EQ_U64(gs_of(&e)->crate_stats.settled, 3);
    sim_free(&e.sim);
}

static void test_crates_snap_to_the_column_they_overlap_most(void) {
    struct {
        f32 x;
        i32 col;
    } cases[] = {{96, 6}, {100, 6}, {103.9f, 6}, {104.5f, 7}, {110, 7}, {111.9f, 7}};
    for (usize i = 0; i < ARRAY_LEN(cases); i++) {
        TestMap m = base_map();
        Env e;
        CHECK(env_init(&e, &m, NULL));
        spawn_free(&e, CRATE_RED, cases[i].x, 100, CRATE_FALLING);
        ticks(&e, 0, 60);
        if (!is_resting_at(&e, cases[i].col, 9)) {
            fprintf(stderr, "crate at x=%.1f should rest in column %d\n", (double)cases[i].x,
                    (int)cases[i].col);
            g_failures++;
        }
        CHECK_EQ_U64(resting_count(&e), 1);
        sim_free(&e.sim);
    }
}

static void test_crate_on_a_ledge_goes_to_the_supported_column(void) {
    // Floor only under columns 0..9: a crate landing mostly over the void settles on the ledge.
    const f32 xs[] = {152.0f, 156.0f, 159.0f};
    for (usize i = 0; i < ARRAY_LEN(xs); i++) {
        TestMap m;
        map_init(&m, 20, 12);
        map_rect(&m, 0, 10, 9, 11, '#');
        map_put(&m, 1, 9, '1');
        Env e;
        CHECK(env_init(&e, &m, NULL));
        spawn_free(&e, CRATE_RED, xs[i], 100, CRATE_FALLING);
        ticks(&e, 0, 60);
        CHECK(is_resting_at(&e, 9, 9));
        sim_free(&e.sim);
    }
    // Entirely over the void: lost.
    TestMap m;
    map_init(&m, 20, 12);
    map_rect(&m, 0, 10, 9, 11, '#');
    map_put(&m, 1, 9, '1');
    Env e;
    CHECK(env_init(&e, &m, NULL));
    spawn_free(&e, CRATE_RED, 200, 100, CRATE_FALLING);
    ticks(&e, 0, 120);
    CHECK_EQ_U64(crate_count(gs_of(&e)), 0);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.destroyed, 1);
    sim_free(&e.sim);
}

static void test_crate_rests_on_a_thin_platform(void) {
    TestMap m = base_map();
    map_rect(&m, 10, 8, 12, 8, '='); // thin platform, top at y = 128
    Env e;
    CHECK(env_init(&e, &m, NULL));
    spawn_free(&e, CRATE_BLUE, 160, 0, CRATE_FALLING);
    ticks(&e, 0, 120);
    CHECK(is_resting_at(&e, 10, 7)); // landed on top of it, not through it
    sim_free(&e.sim);
}

static void test_water_destroys_crates(void) {
    TestMap m = base_map();
    map_rect(&m, 8, 10, 11, 11, '~'); // pit, x in [128,192)
    Env e;
    CHECK(env_init(&e, &m, NULL));
    spawn_resting(&e, CRATE_RED, 7, 9);                 // right beside the pit, on solid floor
    spawn_free(&e, CRATE_RED, 136, 40, CRATE_FALLING);  // over the pit
    ticks(&e, 0, 100);
    CHECK_EQ_U64(crate_count(gs_of(&e)), 1);
    CHECK(is_resting_at(&e, 7, 9));
    CHECK_EQ_U64(gs_of(&e)->crate_stats.destroyed, 1);
    sim_free(&e.sim);
}

static void test_support_loss_makes_a_stack_collapse(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    EntityId bottom = spawn_resting(&e, CRATE_RED, 6, 9);
    spawn_resting(&e, CRATE_RED, 6, 8);
    spawn_resting(&e, CRATE_BLUE, 6, 7);
    ticks(&e, 0, 30);
    CHECK_EQ_U64(resting_count(&e), 3); // a stable stack stays put

    crate_destroy(gs_of(&e), bottom);
    CHECK(!level_dyn_solid(&e.sim.level, 6, 9)); // the grid is updated immediately
    ticks(&e, 0, 60);
    CHECK(is_resting_at(&e, 6, 9)); // the other two dropped one row
    CHECK(is_resting_at(&e, 6, 8));
    CHECK(!level_dyn_solid(&e.sim.level, 6, 7));
    CHECK_EQ_U64(resting_count(&e), 2);
    CHECK(resting_at(&e, 6, 8)->as.crate.kind == CRATE_BLUE);
    sim_free(&e.sim);
}

static void test_crate_waits_while_the_player_stands_in_its_cell(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    teleport(&e, 104, 160); // body x in [98,110]: inside column 6
    EntityId id = spawn_free(&e, CRATE_RED, 96, 0, CRATE_DESCENDING);
    ticks(&e, 0, 260);
    CHECK(crate_of(&e, id) != NULL);
    CHECK(crate_of(&e, id)->as.crate.state != CRATE_RESTING); // waiting on the ground
    CHECK(!level_dyn_solid(&e.sim.level, 6, 9));
    CHECK_NEAR(game_player(gs_of(&e))->body.pos.x, 98.0, 1e-3); // and the player was not shoved
    ticks(&e, KEY(ACT_RIGHT), 40); // walk away
    CHECK(is_resting_at(&e, 6, 9));
    sim_free(&e.sim);
}

// ---- Resting crates are solid ----------------------------------------------------------------

static void test_player_is_blocked_by_a_crate_and_can_climb_a_low_stack(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    spawn_resting(&e, CRATE_RED, 8, 9);
    ticks(&e, KEY(ACT_RIGHT), 120);
    CHECK_NEAR(game_player(gs_of(&e))->body.pos.x, 8.0f * TILE_SIZE - TUNING_COMPANION_W, 1e-3);

    // A walker hops a 1-high crate and a 2-high stack.
    Walker w;
    walker_init(&w, 200.0f);
    InputFrame in = {0};
    bool arrived = false;
    for (int i = 0; i < 400 && !arrived; i++) {
        walker_think(&w, &e.sim.level, game_player(gs_of(&e)), &in);
        sim_tick(&e.sim, &in);
        arrived = walker_arrived(&w, game_player(gs_of(&e))) && game_player(gs_of(&e))->body.on_ground;
    }
    CHECK(arrived);
    sim_free(&e.sim);

    CHECK(env_init(&e, &m, NULL));
    spawn_resting(&e, CRATE_RED, 8, 9);
    spawn_resting(&e, CRATE_RED, 8, 8);
    walker_init(&w, 200.0f);
    memset(&in, 0, sizeof(in));
    arrived = false;
    for (int i = 0; i < 500 && !arrived; i++) {
        walker_think(&w, &e.sim.level, game_player(gs_of(&e)), &in);
        sim_tick(&e.sim, &in);
        arrived = walker_arrived(&w, game_player(gs_of(&e))) && game_player(gs_of(&e))->body.on_ground;
    }
    CHECK(arrived);
    sim_free(&e.sim);
}

static void test_a_tall_stack_is_a_wall(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    for (i32 row = 9; row >= 6; row--) {
        spawn_resting(&e, CRATE_RED, 8, row); // 4 high = 64 px, more than a jump
    }
    Walker w;
    walker_init(&w, 200.0f);
    InputFrame in = {0};
    for (int i = 0; i < 600; i++) {
        walker_think(&w, &e.sim.level, game_player(gs_of(&e)), &in);
        sim_tick(&e.sim, &in);
        CHECK(game_player(gs_of(&e))->body.pos.x <= 8.0f * TILE_SIZE - TUNING_COMPANION_W + 1e-3f);
    }
    CHECK(walker_stuck(&w));
    sim_free(&e.sim);
}

static void test_player_stands_on_a_crate(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    spawn_resting(&e, CRATE_RED, 6, 9);
    teleport(&e, 104, 144); // feet on top of the crate
    CHECK(game_player(gs_of(&e))->body.on_ground);
    CHECK_NEAR(player_feet(&e), 144.0, 1e-3);
    ticks(&e, 0, 30);
    CHECK_NEAR(player_feet(&e), 144.0, 1e-3);
    // Standing on it does not let you pick it up.
    press(&e, KEY(ACT_GRAB));
    CHECK(!entity_id_valid(game_player(gs_of(&e))->carrying));
    sim_free(&e.sim);
}

// ---- Picking up ------------------------------------------------------------------------------

static void test_pick_up_and_carry(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    EntityId id = spawn_resting(&e, CRATE_BLUE, 6, 9);
    teleport(&e, 88, 160); // body right edge at 94, the crate starts at 96
    tick(&e, KEY(ACT_GRAB));
    CHECK(entity_id_eq(game_player(gs_of(&e))->carrying, id));
    CHECK(crate_of(&e, id)->as.crate.state == CRATE_CARRIED);
    CHECK(!level_dyn_solid(&e.sim.level, 6, 9)); // no longer solid
    CHECK_EQ_U64(gs_of(&e)->crate_stats.picked_up, 1);

    tick(&e, 0);
    const Body *pb = &game_player(gs_of(&e))->body;
    CHECK_NEAR(crate_of(&e, id)->body.pos.x, pb->pos.x + pb->size.x * 0.5f - 8.0f, 1e-3);
    CHECK_NEAR(crate_of(&e, id)->body.pos.y, pb->pos.y - 16.0f, 1e-3); // right above the head
    CHECK(crate_of(&e, id)->as.crate.kind == CRATE_BLUE);              // keeps its colour
    sim_free(&e.sim);
}

static void test_pick_up_reach_and_direction(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    spawn_resting(&e, CRATE_RED, 8, 9); // left edge at 128
    teleport(&e, 88, 160);              // 34 px away: out of reach
    press(&e, KEY(ACT_GRAB));
    CHECK(!entity_id_valid(game_player(gs_of(&e))->carrying));

    // A crate behind the player is not picked (facing right, crate on the left).
    spawn_resting(&e, CRATE_RED, 4, 9); // right edge at 80, body starts at 82
    press(&e, KEY(ACT_GRAB));
    CHECK(!entity_id_valid(game_player(gs_of(&e))->carrying));
    game_player(gs_of(&e))->facing = -1; // turn around: now it is
    press(&e, KEY(ACT_GRAB));
    CHECK(entity_id_valid(game_player(gs_of(&e))->carrying));
    CHECK(!level_dyn_solid(&e.sim.level, 4, 9));
    sim_free(&e.sim);
}

static void test_only_the_top_of_a_stack_can_be_picked(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    spawn_resting(&e, CRATE_RED, 6, 9);
    EntityId top = spawn_resting(&e, CRATE_BLUE, 6, 8);
    teleport(&e, 88, 160);
    press(&e, KEY(ACT_GRAB));
    CHECK(entity_id_eq(game_player(gs_of(&e))->carrying, top)); // the upper one
    CHECK(is_resting_at(&e, 6, 9));                       // the lower one is untouched
    CHECK(!level_dyn_solid(&e.sim.level, 6, 8));
    sim_free(&e.sim);

    // A stack of three is out of reach from the ground: nothing can be taken.
    CHECK(env_init(&e, &m, NULL));
    spawn_resting(&e, CRATE_RED, 6, 9);
    spawn_resting(&e, CRATE_RED, 6, 8);
    spawn_resting(&e, CRATE_RED, 6, 7);
    teleport(&e, 88, 160);
    press(&e, KEY(ACT_GRAB));
    CHECK(!entity_id_valid(game_player(gs_of(&e))->carrying));
    CHECK_EQ_U64(resting_count(&e), 3);
    sim_free(&e.sim);
}

static void test_carrying_slows_and_lowers_the_jump(void) {
    TestMap m = base_map();
    map_rect(&m, 15, 6, 15, 9, 'H'); // a ladder
    Env e;
    // Without a crate: full speed.
    CHECK(env_init(&e, &m, NULL));
    teleport(&e, 88, 160);
    ticks(&e, KEY(ACT_RIGHT), 20);
    CHECK_NEAR(game_player(gs_of(&e))->body.vel.x, TUNING_WALK_SPEED, 1e-3);
    sim_free(&e.sim);

    // With one in the hands: slower.
    CHECK(env_init(&e, &m, NULL));
    spawn_resting(&e, CRATE_RED, 6, 9);
    teleport(&e, 88, 160);
    press(&e, KEY(ACT_GRAB));
    CHECK(entity_id_valid(game_player(gs_of(&e))->carrying));
    ticks(&e, KEY(ACT_RIGHT), 20);
    CHECK_NEAR(game_player(gs_of(&e))->body.vel.x, TUNING_WALK_SPEED * TUNING_CARRY_SPEED_MULT, 1e-3);

    // Lower jump: (300 * 0.8)^2 / 1800 = 32 px, minus discretisation.
    ticks(&e, 0, 20);
    const f32 start = game_player(gs_of(&e))->body.pos.y;
    f32 top = start;
    for (int i = 0; i < 70; i++) {
        tick(&e, i < 60 ? KEY(ACT_JUMP) : 0);
        top = MIN(top, game_player(gs_of(&e))->body.pos.y);
    }
    CHECK(start - top > 27.0f && start - top < 33.0f);
    CHECK(entity_id_valid(game_player(gs_of(&e))->carrying)); // still holding it

    sim_free(&e.sim);

    // No ladder while carrying; fine again once the crate is put down.
    CHECK(env_init(&e, &m, NULL));
    spawn_resting(&e, CRATE_RED, 16, 9);
    teleport(&e, 15.0f * TILE_SIZE + 8.0f, 160);
    press(&e, KEY(ACT_GRAB));
    CHECK(entity_id_valid(game_player(gs_of(&e))->carrying));
    ticks(&e, KEY(ACT_UP), 20);
    CHECK(!game_player(gs_of(&e))->on_ladder);
    press(&e, KEY(ACT_GRAB)); // put it down
    CHECK(!entity_id_valid(game_player(gs_of(&e))->carrying));
    ticks(&e, KEY(ACT_UP), 10);
    CHECK(game_player(gs_of(&e))->on_ladder);
    sim_free(&e.sim);
}

// ---- Placing ---------------------------------------------------------------------------------

static void test_place_in_front(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    spawn_resting(&e, CRATE_BLUE, 6, 9);
    teleport(&e, 88, 160);
    press(&e, KEY(ACT_GRAB)); // pick it up
    CHECK(entity_id_valid(game_player(gs_of(&e))->carrying));

    tick(&e, 0);
    tick(&e, KEY(ACT_GRAB)); // put it down: first column entirely in front of the body
    CHECK(!entity_id_valid(game_player(gs_of(&e))->carrying));
    CHECK(is_resting_at(&e, 6, 9)); // placed and settled within the same tick
    CHECK_EQ_U64(gs_of(&e)->crate_stats.placed, 1);
    CHECK(resting_at(&e, 6, 9)->as.crate.kind == CRATE_BLUE);
    sim_free(&e.sim);

    // Facing left, the crate goes in the first column entirely to the left of the body.
    CHECK(env_init(&e, &m, NULL));
    spawn_resting(&e, CRATE_RED, 4, 9);
    teleport(&e, 88, 160);
    game_player(gs_of(&e))->facing = -1;
    press(&e, KEY(ACT_GRAB)); // takes the crate at (4,9), which is in front of us
    CHECK(entity_id_valid(game_player(gs_of(&e))->carrying));
    CHECK(!level_dyn_solid(&e.sim.level, 4, 9));
    tick(&e, KEY(ACT_GRAB)); // and puts it back: body left edge is 82, so column 4 (right edge 80)
    CHECK(is_resting_at(&e, 4, 9));
    sim_free(&e.sim);
}

static void test_place_builds_stacks_up_to_the_raise_limit(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    spawn_resting(&e, CRATE_RED, 6, 9);
    teleport(&e, 88, 160);
    // Carry three more crates, one after the other, placing each in front.
    for (i32 n = 0; n < 3; n++) {
        const EntityId c = spawn_free(&e, CRATE_RED, 40, 0, CRATE_CARRIED);
        game_player(gs_of(&e))->carrying = c;
        press(&e, KEY(ACT_GRAB));
        if (n < 2) {
            CHECK(!entity_id_valid(game_player(gs_of(&e))->carrying));
        }
    }
    CHECK(is_resting_at(&e, 6, 9));
    CHECK(is_resting_at(&e, 6, 8));
    CHECK(is_resting_at(&e, 6, 7));
    // The third one could not be placed (4th level is out of the raise limit): still carried.
    CHECK(entity_id_valid(game_player(gs_of(&e))->carrying));
    CHECK_EQ_U64(resting_count(&e), 3);
    sim_free(&e.sim);
}

static void test_place_is_refused_against_a_wall(void) {
    TestMap m = base_map();
    map_rect(&m, 6, 5, 6, 9, '#'); // a wall right in front
    Env e;
    CHECK(env_init(&e, &m, NULL));
    teleport(&e, 88, 160);
    game_player(gs_of(&e))->carrying = spawn_free(&e, CRATE_RED, 40, 0, CRATE_CARRIED);
    press(&e, KEY(ACT_GRAB));
    CHECK(entity_id_valid(game_player(gs_of(&e))->carrying)); // nowhere to put it
    CHECK_EQ_U64(gs_of(&e)->crate_stats.placed, 0);
    sim_free(&e.sim);
}

// ---- Throwing --------------------------------------------------------------------------------

static void test_throw_lobs_in_the_facing_direction(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    teleport(&e, 40, 160);
    EntityId id = spawn_free(&e, CRATE_RED, 0, 0, CRATE_CARRIED);
    game_player(gs_of(&e))->carrying = id;
    tick(&e, KEY(ACT_FIRE));
    CHECK(!entity_id_valid(game_player(gs_of(&e))->carrying));
    CHECK(crate_of(&e, id)->as.crate.state == CRATE_FALLING);
    CHECK(crate_of(&e, id)->body.vel.x > 0.0f && crate_of(&e, id)->body.vel.y < 0.0f);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.thrown, 1);

    ticks(&e, 0, 120);
    CHECK_EQ_U64(resting_count(&e), 1);
    // Lands about 95 px ahead of the thrower: column 7.
    CHECK(is_resting_at(&e, 7, 9) || is_resting_at(&e, 6, 9) || is_resting_at(&e, 8, 9));
    sim_free(&e.sim);

    // Facing left.
    CHECK(env_init(&e, &m, NULL));
    teleport(&e, 200, 160);
    game_player(gs_of(&e))->facing = -1;
    game_player(gs_of(&e))->carrying = spawn_free(&e, CRATE_RED, 0, 0, CRATE_CARRIED);
    tick(&e, KEY(ACT_FIRE));
    CHECK(gs_of(&e)->entities.slots[1].body.vel.x < 0.0f);
    ticks(&e, 0, 120);
    CHECK(is_resting_at(&e, 5, 9) || is_resting_at(&e, 6, 9) || is_resting_at(&e, 7, 9));
    sim_free(&e.sim);
}

static void test_thrown_crate_stops_at_a_wall(void) {
    TestMap m = base_map();
    map_rect(&m, 8, 4, 8, 9, '#'); // wall x in [128,144)
    Env e;
    CHECK(env_init(&e, &m, NULL));
    teleport(&e, 40, 160);
    game_player(gs_of(&e))->carrying = spawn_free(&e, CRATE_RED, 0, 0, CRATE_CARRIED);
    tick(&e, KEY(ACT_FIRE));
    ticks(&e, 0, 120);
    CHECK(is_resting_at(&e, 7, 9)); // right up against the wall, never through it
    sim_free(&e.sim);
}

static void test_throw_is_blocked_by_a_low_ceiling(void) {
    TestMap m = base_map();
    map_rect(&m, 0, 7, 19, 7, '#'); // ceiling: underside at y = 128; head + crate need y >= 122
    Env e;
    CHECK(env_init(&e, &m, NULL));
    teleport(&e, 40, 160);
    game_player(gs_of(&e))->carrying = spawn_free(&e, CRATE_RED, 0, 0, CRATE_CARRIED);
    press(&e, KEY(ACT_FIRE));
    CHECK(entity_id_valid(game_player(gs_of(&e))->carrying)); // refused
    CHECK_EQ_U64(gs_of(&e)->crate_stats.thrown, 0);
    press(&e, KEY(ACT_GRAB)); // placing still works
    CHECK(!entity_id_valid(game_player(gs_of(&e))->carrying));
    sim_free(&e.sim);
}

// ---- Losing the crate ------------------------------------------------------------------------

static void test_falling_in_water_loses_the_carried_crate(void) {
    TestMap m = base_map();
    map_rect(&m, 8, 10, 11, 11, '~');
    Env e;
    CHECK(env_init(&e, &m, NULL));
    EntityId id = spawn_resting(&e, CRATE_RED, 2, 9);
    teleport(&e, 40, 160);
    game_player(gs_of(&e))->facing = -1;
    press(&e, KEY(ACT_GRAB));
    CHECK(entity_id_eq(game_player(gs_of(&e))->carrying, id));
    ticks(&e, KEY(ACT_RIGHT), 200); // walks into the pit, respawns
    CHECK_EQ_U64(gs_of(&e)->respawns >= 1, 1);
    CHECK(!entity_id_valid(game_player(gs_of(&e))->carrying));
    CHECK(crate_of(&e, id) == NULL); // the old id is stale
    CHECK_EQ_U64(crate_count(gs_of(&e)), 0);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.destroyed, 1);
    sim_free(&e.sim);
}

// ---- Breaking crates for hearts or ammunition --------------------------------------------------

static const GameEvent *salvage_event(Env *e) {
    const EventQueue *q = &gs_of(e)->events;
    for (u32 i = 0; i < q->count; i++) {
        if (q->items[i].type == EV_CRATE_SALVAGED) {
            return &q->items[i];
        }
    }
    return NULL;
}

// A press of the destroy key; returns what it gave (a copy of the event), NULL if it broke nothing.
static const GameEvent *break_crate(Env *e) {
    static GameEvent seen;
    tick(e, KEY(ACT_DESTROY));
    const GameEvent *ev = salvage_event(e);
    const bool broke = ev != NULL;
    if (broke) {
        seen = *ev;
    }
    tick(e, 0);
    return broke ? &seen : NULL;
}

// The player stands left of a crate at column 6 with a pistol holding `ammo` shots and `hp` hit points.
static void prepare_salvage(Env *e, WeaponKind weapon, i32 ammo, i32 hp) {
    Companion *p = game_player(gs_of(e));
    combat_give_weapon(gs_of(e), weapon);
    p->ammo = weapon == WEAPON_FISTS ? 0 : ammo;
    p->hp = hp;
}

static void test_destroy_breaks_the_crate_in_front_for_a_reward(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    spawn_resting(&e, CRATE_RED, 6, 9);
    teleport(&e, 88, 160);
    prepare_salvage(&e, WEAPON_PISTOL, 5, 50);
    tick(&e, KEY(ACT_DESTROY));
    const GameEvent *ev = salvage_event(&e);
    CHECK(ev != NULL);
    CHECK(!is_resting_at(&e, 6, 9) && !level_dyn_solid(&e.sim.level, 6, 9));
    CHECK_EQ_U64(gs_of(&e)->crate_stats.salvaged, 1);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.destroyed, 0); // it is not a loss
    if (ev && ev->a == SALVAGE_HEARTS) {
        CHECK_EQ_U64(ev->b, TUNING_SALVAGE_HEAL);
        CHECK_EQ_U64(game_player(gs_of(&e))->hp, 50 + TUNING_SALVAGE_HEAL);
        CHECK_EQ_U64(game_player(gs_of(&e))->ammo, 5);
    } else if (ev) {
        CHECK_EQ_U64(ev->a, SALVAGE_AMMO);
        CHECK_EQ_U64(ev->b, weapon_def(WEAPON_PISTOL)->max_ammo * TUNING_SALVAGE_AMMO_PERCENT / 100);
        CHECK_EQ_U64(game_player(gs_of(&e))->ammo, 5 + ev->b);
        CHECK_EQ_U64(game_player(gs_of(&e))->hp, 50);
    }
    // Held down, the key breaks one crate per press, not one per tick.
    spawn_resting(&e, CRATE_RED, 6, 9);
    spawn_resting(&e, CRATE_RED, 6, 8);
    ticks(&e, KEY(ACT_DESTROY), 10);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.salvaged, 1);
    sim_free(&e.sim);
}

static void test_destroy_gives_hearts_and_ammunition_at_random(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    teleport(&e, 88, 160);
    u32 hearts = 0, ammo = 0;
    for (int i = 0; i < 40; i++) {
        spawn_resting(&e, CRATE_RED, 6, 9);
        ticks(&e, 0, 2);
        prepare_salvage(&e, WEAPON_PISTOL, 5, 50);
        const GameEvent *ev = break_crate(&e);
        hearts += ev && ev->a == SALVAGE_HEARTS ? 1u : 0u;
        ammo += ev && ev->a == SALVAGE_AMMO ? 1u : 0u;
    }
    CHECK(hearts + ammo == 40 && hearts >= 8 && ammo >= 8);
    sim_free(&e.sim);
}

static void test_destroy_never_wastes_a_reward_when_the_other_helps(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    teleport(&e, 88, 160);
    const i32 full = game_player(gs_of(&e))->max_hp;
    for (int i = 0; i < 20; i++) { // full health: only ammunition is useful
        spawn_resting(&e, CRATE_RED, 6, 9);
        ticks(&e, 0, 2);
        prepare_salvage(&e, WEAPON_PISTOL, 5, full);
        const GameEvent *ev = break_crate(&e);
        CHECK(ev && ev->a == SALVAGE_AMMO);
    }
    for (int i = 0; i < 20; i++) { // fists (no ammunition to give): only hearts, no more than what is missing
        spawn_resting(&e, CRATE_RED, 6, 9);
        ticks(&e, 0, 2);
        prepare_salvage(&e, WEAPON_FISTS, 0, full - 10);
        const GameEvent *ev = break_crate(&e);
        CHECK(ev && ev->a == SALVAGE_HEARTS && ev->b == 10);
        CHECK_EQ_U64(game_player(gs_of(&e))->hp, full);
    }
    // Nothing to give: the crate is broken all the same, for nothing.
    spawn_resting(&e, CRATE_RED, 6, 9);
    ticks(&e, 0, 2);
    prepare_salvage(&e, WEAPON_FISTS, 0, full);
    const GameEvent *nothing = break_crate(&e);
    CHECK(nothing && nothing->a == SALVAGE_NOTHING && nothing->b == 0);
    CHECK(!is_resting_at(&e, 6, 9));
    sim_free(&e.sim);
}

static void test_destroy_takes_the_top_of_a_stack_and_the_carried_crate(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    spawn_resting(&e, CRATE_RED, 6, 9);
    spawn_resting(&e, CRATE_BLUE, 6, 8);
    teleport(&e, 88, 160);
    prepare_salvage(&e, WEAPON_PISTOL, 5, 50);
    press(&e, KEY(ACT_DESTROY));
    CHECK(is_resting_at(&e, 6, 9) && !is_resting_at(&e, 6, 8)); // the upper one
    // Right after, pick the lower one up and break it from the hands.
    press(&e, KEY(ACT_GRAB));
    CHECK(entity_id_valid(game_player(gs_of(&e))->carrying));
    press(&e, KEY(ACT_DESTROY));
    CHECK(!entity_id_valid(game_player(gs_of(&e))->carrying));
    CHECK_EQ_U64(crate_count(gs_of(&e)), 0);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.salvaged, 2);
    // Nothing in reach: nothing happens.
    CHECK(break_crate(&e) == NULL && gs_of(&e)->crate_stats.salvaged == 2);
    sim_free(&e.sim);
}

// ---- What the player is about to take or put down ---------------------------------------------

static void test_the_targets_say_what_a_press_would_do(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    EntityId id = spawn_resting(&e, CRATE_RED, 6, 9);
    teleport(&e, 40, 160);
    const Companion *p = game_player(gs_of(&e));
    CHECK(!entity_id_valid(crate_pickup_target(gs_of(&e), p))); // too far
    teleport(&e, 88, 160);
    CHECK(entity_id_eq(crate_pickup_target(gs_of(&e), p), id));
    i32 col, row;
    CHECK(!crate_place_target(gs_of(&e), p, &col, &row)); // nothing to put down
    press(&e, KEY(ACT_GRAB));
    CHECK(!entity_id_valid(crate_pickup_target(gs_of(&e), p))); // hands full
    CHECK(crate_place_target(gs_of(&e), p, &col, &row) && col == 6 && row == 9); // where a press would put it
    press(&e, KEY(ACT_GRAB));
    CHECK(is_resting_at(&e, 6, 9));
    sim_free(&e.sim);
}

static void test_a_thrown_crate_leaves_no_instant_shot_when_fire_stays_held(void) {
    TestMap m = base_map();
    Env e;
    CHECK(env_init(&e, &m, NULL));
    teleport(&e, 40, 160);
    combat_give_weapon(gs_of(&e), WEAPON_PISTOL);
    game_player(gs_of(&e))->carrying = spawn_free(&e, CRATE_RED, 0, 0, CRATE_CARRIED);
    ticks(&e, KEY(ACT_FIRE), 12); // the first tick throws the crate, the others keep the key down
    CHECK_EQ_U64(gs_of(&e)->crate_stats.thrown, 1);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 0);
    ticks(&e, KEY(ACT_FIRE), 30);
    CHECK(gs_of(&e)->combat_stats.shots >= 1); // then the pistol fires again
    sim_free(&e.sim);
}

// ---- Drop zones ------------------------------------------------------------------------------

static void test_drop_zone_cadence_and_cap(void) {
    TestMap m = base_map();
    map_put(&m, 3, 0, 'D');
    Env e;
    CHECK(env_init(&e, &m, "drop.1=red,2,2\n"));
    ticks(&e, 0, 119);
    CHECK_EQ_U64(crate_count(gs_of(&e)), 0); // nothing before the first interval
    ticks(&e, 0, 6);
    CHECK_EQ_U64(crate_count(gs_of(&e)), 1);
    const Entity *first = NULL;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        if (gs_of(&e)->entities.slots[i].type == ENT_CRATE) {
            first = &gs_of(&e)->entities.slots[i];
        }
    }
    CHECK(first != NULL);
    CHECK(first->as.crate.state == CRATE_DESCENDING);
    CHECK(first->as.crate.kind == CRATE_RED);
    CHECK_EQ_U64(first->as.crate.origin_zone, 0);
    CHECK_NEAR(first->body.pos.x, 3.0f * TILE_SIZE, 0.0); // column-aligned under the marker

    // Second one two seconds later, next to the first rather than on top of it; then the cap of 2 holds.
    ticks(&e, 0, 1200);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.spawned, 2);
    CHECK(is_resting_at(&e, 3, 9));
    CHECK(is_resting_at(&e, 2, 9) != is_resting_at(&e, 4, 9));
    CHECK(!is_resting_at(&e, 3, 8));

    // Free a slot: the cadence resumes after one interval, not instantly.
    crate_destroy(gs_of(&e), (EntityId){0, 0}); // no-op on an invalid id
    const Entity *top = resting_at(&e, is_resting_at(&e, 2, 9) ? 2 : 4, 9);
    CHECK(top != NULL);
    crate_destroy(gs_of(&e), (EntityId){(u16)(top - gs_of(&e)->entities.slots), top->generation});
    ticks(&e, 0, 100);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.spawned, 2);
    ticks(&e, 0, 40);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.spawned, 3);
    sim_free(&e.sim);
}

static void test_drop_zone_colors(void) {
    TestMap m = base_map();
    map_put(&m, 3, 0, 'D');
    Env e;
    CHECK(env_init(&e, &m, "drop.1=blue,1,50\n"));
    u32 blue = 0, red = 0;
    for (int i = 0; i < 600; i++) {
        tick(&e, 0);
        for (u16 s = 1; s <= MAX_ENTITIES; s++) {
            const Entity *c = &gs_of(&e)->entities.slots[s];
            if (c->type == ENT_CRATE) {
                if (c->as.crate.kind == CRATE_BLUE) {
                    blue++;
                } else {
                    red++;
                }
                crate_destroy(gs_of(&e), (EntityId){s, c->generation}); // keep the sky clear
            }
        }
    }
    CHECK(blue >= 5 && red == 0); // a blue zone only drops blue crates
    sim_free(&e.sim);

    // Mixed with 50%: both colours show up, in roughly equal numbers.
    CHECK(env_init(&e, &m, "drop.1=mixed,1,50,50\n"));
    blue = red = 0;
    for (int i = 0; i < 6100; i++) {
        tick(&e, 0);
        for (u16 s = 1; s <= MAX_ENTITIES; s++) {
            const Entity *c = &gs_of(&e)->entities.slots[s];
            if (c->type == ENT_CRATE && !c->pending_destroy) {
                if (c->as.crate.kind == CRATE_BLUE) {
                    blue++;
                } else {
                    red++;
                }
                crate_destroy(gs_of(&e), (EntityId){s, c->generation});
            }
        }
    }
    CHECK(blue + red >= 95 && blue + red <= 101);
    CHECK(blue > 30 && red > 30);
    sim_free(&e.sim);
}

static void test_mixed_zone_default_blue_share_is_a_minority(void) {
    TestMap m = base_map();
    map_put(&m, 3, 0, 'D');
    Env e;
    CHECK(env_init(&e, &m, "drop.1=mixed,1,50\n")); // default 25% blue
    u32 blue = 0, total = 0;
    for (int i = 0; i < 12100; i++) {
        tick(&e, 0);
        for (u16 s = 1; s <= MAX_ENTITIES; s++) {
            const Entity *c = &gs_of(&e)->entities.slots[s];
            if (c->type == ENT_CRATE && !c->pending_destroy) {
                total++;
                blue += c->as.crate.kind == CRATE_BLUE;
                crate_destroy(gs_of(&e), (EntityId){s, c->generation});
            }
        }
    }
    CHECK(total >= 190);
    CHECK(blue * 100 > total * 12 && blue * 100 < total * 40); // about a quarter
    sim_free(&e.sim);
}

int main(void) {
    RUN_TEST(test_descent_is_slow_and_constant);
    RUN_TEST(test_stacking_from_the_bottom_up);
    RUN_TEST(test_crates_snap_to_the_column_they_overlap_most);
    RUN_TEST(test_crate_on_a_ledge_goes_to_the_supported_column);
    RUN_TEST(test_crate_rests_on_a_thin_platform);
    RUN_TEST(test_water_destroys_crates);
    RUN_TEST(test_support_loss_makes_a_stack_collapse);
    RUN_TEST(test_crate_waits_while_the_player_stands_in_its_cell);
    RUN_TEST(test_player_is_blocked_by_a_crate_and_can_climb_a_low_stack);
    RUN_TEST(test_a_tall_stack_is_a_wall);
    RUN_TEST(test_player_stands_on_a_crate);
    RUN_TEST(test_pick_up_and_carry);
    RUN_TEST(test_pick_up_reach_and_direction);
    RUN_TEST(test_only_the_top_of_a_stack_can_be_picked);
    RUN_TEST(test_carrying_slows_and_lowers_the_jump);
    RUN_TEST(test_place_in_front);
    RUN_TEST(test_place_builds_stacks_up_to_the_raise_limit);
    RUN_TEST(test_place_is_refused_against_a_wall);
    RUN_TEST(test_throw_lobs_in_the_facing_direction);
    RUN_TEST(test_thrown_crate_stops_at_a_wall);
    RUN_TEST(test_throw_is_blocked_by_a_low_ceiling);
    RUN_TEST(test_falling_in_water_loses_the_carried_crate);
    RUN_TEST(test_destroy_breaks_the_crate_in_front_for_a_reward);
    RUN_TEST(test_destroy_gives_hearts_and_ammunition_at_random);
    RUN_TEST(test_destroy_never_wastes_a_reward_when_the_other_helps);
    RUN_TEST(test_destroy_takes_the_top_of_a_stack_and_the_carried_crate);
    RUN_TEST(test_the_targets_say_what_a_press_would_do);
    RUN_TEST(test_a_thrown_crate_leaves_no_instant_shot_when_fire_stays_held);
    RUN_TEST(test_drop_zone_cadence_and_cap);
    RUN_TEST(test_drop_zone_colors);
    RUN_TEST(test_mixed_zone_default_blue_share_is_a_minority);
    TEST_MAIN_END();
}
