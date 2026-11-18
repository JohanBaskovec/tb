// Navigation: the graph built by simulating the physics, A*, and the path follower.
#include "test.h"

#include "core/arena.h"
#include "core/rng.h"
#include "game/nav.h"
#include "game/pad.h"
#include "game/tuning.h"
#include "test_helpers.h"

static Nav g_nav; // big: keep it off the stack

typedef struct Fixture {
    Arena arena;
    Level lv;
} Fixture;

static bool fixture_map(Fixture *f, const TestMap *m) {
    if (!arena_init(&f->arena, 1u << 20)) {
        return false;
    }
    if (!map_load(m, &f->lv, &f->arena)) {
        arena_free(&f->arena);
        return false;
    }
    nav_build(&g_nav, &f->lv);
    return true;
}

static void fixture_free(Fixture *f) {
    arena_free(&f->arena);
}

static i32 ground(i32 tx, i32 ty) {
    return nav_find(&g_nav, tx, ty, NAV_GROUND);
}

static const NavEdge *edge_between(i32 from, i32 to, bool carrying) {
    const NavNode *n = &g_nav.nodes[from];
    for (u32 k = 0; k < n->edge_count; k++) {
        const NavEdge *e = &g_nav.edges[n->first_edge + k];
        if (e->to == to && (e->who & (carrying ? NAV_CARRY : NAV_FREE))) {
            return e;
        }
    }
    return NULL;
}

// Flat floor: the ground cell row is 5 (floor rows 6-7).
static void flat_map(TestMap *m, int w) {
    map_init(m, w, 8);
    map_rect(m, 0, 6, w - 1, 7, '#');
}

// ---- Graph -------------------------------------------------------------------------------------

static void test_a_flat_floor_is_a_row_of_nodes_joined_by_walking(void) {
    TestMap m;
    flat_map(&m, 20);
    Fixture f;
    CHECK(fixture_map(&f, &m));
    CHECK(g_nav.ok);
    CHECK_EQ_U64(g_nav.node_count, 20);
    const NavEdge *e = edge_between(ground(4, 5), ground(5, 5), false);
    CHECK(e != NULL);
    if (e) {
        CHECK_EQ_U64(e->kind, NAV_WALK);
        CHECK_EQ_U64(e->dir, 1);
        CHECK((e->who & NAV_CARRY) != 0);
    }
    CHECK(edge_between(ground(5, 5), ground(4, 5), false) != NULL);
    // Nothing is a node where the body would not fit: inside the floor.
    CHECK_EQ_U64(nav_find(&g_nav, 4, 6, NAV_GROUND), (u64)-1);
    fixture_free(&f);
}

static u32 kinds_in(const u16 *path, i32 n, NavEdgeKind kind) {
    u32 count = 0;
    for (i32 i = 0; i < n; i++) {
        count += g_nav.edges[path[i]].kind == kind ? 1u : 0u;
    }
    return count;
}

static void test_a_step_up_is_a_jump_and_a_step_down_a_walk(void) {
    TestMap m;
    flat_map(&m, 20);
    map_rect(&m, 10, 5, 19, 5, '#'); // one tile higher from column 10 on
    Fixture f;
    CHECK(fixture_map(&f, &m));
    u16 path[NAV_PATH_MAX];
    const i32 up = nav_path(&g_nav, ground(3, 5), ground(15, 4), false, -1, path, NAV_PATH_MAX);
    CHECK(up > 0);
    CHECK(kinds_in(path, up, NAV_JUMP) >= 1);
    const i32 down = nav_path(&g_nav, ground(15, 4), ground(3, 5), false, -1, path, NAV_PATH_MAX);
    CHECK(down > 0);
    CHECK_EQ_U64(kinds_in(path, down, NAV_WALK), down); // walking off the step is enough
    // A step is no obstacle for a crate carrier either (lower jump, still a tile).
    CHECK(nav_path(&g_nav, ground(3, 5), ground(15, 4), true, -1, path, NAV_PATH_MAX) > 0);
    fixture_free(&f);
}

static void test_a_gap_is_crossed_by_a_jump_and_a_wide_one_is_not(void) {
    TestMap m;
    flat_map(&m, 40);
    map_rect(&m, 8, 6, 9, 7, '.');   // 2 wide
    map_rect(&m, 24, 6, 33, 7, '.'); // 10 wide
    Fixture f;
    CHECK(fixture_map(&f, &m));
    const NavEdge *e = edge_between(ground(7, 5), ground(10, 5), false);
    CHECK(e != NULL);
    if (e) {
        CHECK_EQ_U64(e->kind, NAV_JUMP);
    }
    CHECK(nav_find(&g_nav, 8, 5, NAV_GROUND) < 0); // nothing under it
    u16 path[NAV_PATH_MAX];
    CHECK(nav_path(&g_nav, ground(2, 5), ground(15, 5), false, -1, path, NAV_PATH_MAX) > 0);
    CHECK_EQ_U64(nav_path(&g_nav, ground(2, 5), ground(36, 5), false, -1, path, NAV_PATH_MAX), (u64)-1);
    fixture_free(&f);
}

// A ladder from the floor (ground row 9) up to a ledge (ground row 4) too high to jump to.
static void ladder_map(TestMap *m) {
    map_init(m, 24, 12);
    map_rect(m, 0, 10, 23, 11, '#');
    map_rect(m, 6, 5, 20, 5, '#'); // upper floor, from column 6
    map_rect(m, 5, 5, 5, 9, 'H');  // the ladder, its top flush with the upper floor
}

static void test_a_ladder_joins_two_floors_but_not_for_a_crate_carrier(void) {
    TestMap m;
    ladder_map(&m);
    Fixture f;
    CHECK(fixture_map(&f, &m));
    CHECK(nav_find(&g_nav, 5, 7, NAV_LADDER) >= 0);
    u16 path[NAV_PATH_MAX];
    const i32 n = nav_path(&g_nav, ground(2, 9), ground(12, 4), false, -1, path, NAV_PATH_MAX);
    CHECK(n > 0);
    bool climbs = false;
    for (i32 i = 0; i < n; i++) {
        climbs |= g_nav.edges[path[i]].kind == NAV_CLIMB;
    }
    CHECK(climbs);
    // Walking down from the ledge works for everybody; going up carrying a crate does not.
    CHECK(nav_path(&g_nav, ground(2, 9), ground(12, 4), true, -1, path, NAV_PATH_MAX) < 0);
    CHECK(nav_path(&g_nav, ground(12, 4), ground(2, 9), true, -1, path, NAV_PATH_MAX) >= 0);
    fixture_free(&f);
}

static void test_a_thin_platform_is_jumped_onto_and_dropped_from(void) {
    TestMap m;
    flat_map(&m, 20);
    map_init(&m, 20, 12);
    map_rect(&m, 0, 10, 19, 11, '#');
    map_rect(&m, 5, 8, 9, 8, '='); // standing row 7 on it
    Fixture f;
    CHECK(fixture_map(&f, &m));
    u16 path[NAV_PATH_MAX];
    CHECK(nav_path(&g_nav, ground(3, 9), ground(7, 7), false, -1, path, NAV_PATH_MAX) > 0);
    bool drops = false;
    const NavNode *n = &g_nav.nodes[ground(7, 7)];
    for (u32 k = 0; k < n->edge_count; k++) {
        drops |= g_nav.edges[n->first_edge + k].kind == NAV_DROP;
    }
    CHECK(drops);
    fixture_free(&f);
}

static void test_a_wall_too_high_to_jump_separates_the_two_sides(void) {
    TestMap m;
    flat_map(&m, 24);
    map_rect(&m, 12, 0, 12, 5, '#');
    Fixture f;
    CHECK(fixture_map(&f, &m));
    u16 path[NAV_PATH_MAX];
    CHECK_EQ_U64(nav_path(&g_nav, ground(3, 5), ground(20, 5), false, -1, path, NAV_PATH_MAX), (u64)-1);
    CHECK_EQ_U64(nav_path(&g_nav, ground(3, 5), ground(3, 5), false, -1, path, NAV_PATH_MAX), 0);
    fixture_free(&f);
}

static void test_a_path_is_a_chain_of_edges_that_leads_to_the_goal(void) {
    TestMap m;
    ladder_map(&m);
    Fixture f;
    CHECK(fixture_map(&f, &m));
    u16 path[NAV_PATH_MAX];
    const i32 from = ground(1, 9), goal = ground(18, 4);
    const i32 n = nav_path(&g_nav, from, goal, false, -1, path, NAV_PATH_MAX);
    CHECK(n > 0);
    i32 at = from;
    for (i32 i = 0; i < n; i++) {
        CHECK_EQ_U64(g_nav.edges[path[i]].from, at);
        at = g_nav.edges[path[i]].to;
    }
    CHECK_EQ_U64(at, goal);
    // A banned edge is avoided (here the only way up is banned: no path).
    i32 climb = -1;
    for (i32 i = 0; i < n; i++) {
        if (g_nav.edges[path[i]].kind == NAV_CLIMB) {
            climb = path[i];
            break;
        }
    }
    CHECK(climb >= 0);
    fixture_free(&f);
}

static void test_the_graph_is_deterministic_and_rebuilt_alike(void) {
    TestMap m;
    ladder_map(&m);
    Fixture f;
    CHECK(fixture_map(&f, &m));
    static Nav second;
    nav_build(&second, &f.lv);
    CHECK_EQ_U64(second.node_count, g_nav.node_count);
    CHECK_EQ_U64(second.edge_count, g_nav.edge_count);
    CHECK(memcmp(second.nodes, g_nav.nodes, sizeof(NavNode) * g_nav.node_count) == 0);
    for (u32 i = 0; i < g_nav.edge_count; i++) { // field by field: the padding of a struct is indeterminate
        const NavEdge *a = &second.edges[i], *b = &g_nav.edges[i];
        CHECK(a->from == b->from && a->to == b->to && a->kind == b->kind && a->dir == b->dir && a->vert == b->vert &&
              a->hold == b->hold && a->cost == b->cost && a->who == b->who);
    }
    fixture_free(&f);
}

static void test_a_level_without_standing_place_gives_an_unusable_graph(void) {
    TestMap m;
    map_init(&m, 6, 4);
    map_rect(&m, 0, 0, 5, 3, '#');
    Fixture f;
    CHECK(fixture_map(&f, &m));
    CHECK(!g_nav.ok);
    NavAgent a = {0};
    Companion c;
    companion_init(&c, vec2(24, 32));
    u32 mask;
    CHECK_EQ_U64(nav_steer(&g_nav, &a, &f.lv, &c, vec2(40, 32), &mask), NAV_NO_PATH);
    fixture_free(&f);
}

// ---- Following ---------------------------------------------------------------------------------

// Runs a soldier toward `goal` until it arrives; returns the ticks it took, or -1 on timeout.
static i32 travel(const Level *lv, Companion *c, Vec2 goal, i32 max_ticks) {
    NavAgent a;
    memset(&a, 0, sizeof(a));
    InputFrame in = {0};
    for (i32 t = 0; t < max_ticks; t++) {
        u32 mask;
        const NavStatus st = nav_steer(&g_nav, &a, lv, c, goal, &mask);
        if (st == NAV_ARRIVED) {
            return t;
        }
        if (st == NAV_NO_PATH) {
            return -2;
        }
        input_frame_update(&in, mask);
        companion_update(c, lv, &in);
        if (companion_in_hazard(c, lv)) {
            return -3;
        }
    }
    return -1;
}

static void test_the_follower_walks_hops_and_climbs_to_a_goal(void) {
    TestMap m;
    ladder_map(&m);
    Fixture f;
    CHECK(fixture_map(&f, &m));
    Companion c;
    companion_init(&c, vec2(24, 160)); // column 1, floor
    const i32 t = travel(&f.lv, &c, vec2(18 * 16 + 8, 5 * 16), 900);
    CHECK(t > 0);
    CHECK_NEAR(c.body.pos.y + c.body.size.y, 5 * 16, 0.5f); // on the ledge
    fixture_free(&f);
}

static void test_the_follower_goes_back_down(void) {
    TestMap m;
    ladder_map(&m);
    Fixture f;
    CHECK(fixture_map(&f, &m));
    Companion c;
    companion_init(&c, vec2(18 * 16 + 8, 5 * 16));
    const i32 t = travel(&f.lv, &c, vec2(40, 160), 900);
    CHECK(t > 0);
    CHECK_NEAR(c.body.pos.y + c.body.size.y, 160, 0.5f);
    fixture_free(&f);
}

static void test_the_follower_jumps_a_gap_and_a_thin_platform(void) {
    TestMap m;
    map_init(&m, 40, 12);
    map_rect(&m, 0, 10, 39, 11, '#');
    map_rect(&m, 10, 10, 11, 11, '.');
    map_rect(&m, 20, 8, 24, 8, '=');
    Fixture f;
    CHECK(fixture_map(&f, &m));
    Companion c;
    companion_init(&c, vec2(24, 160));
    CHECK(travel(&f.lv, &c, vec2(34 * 16 + 8, 160), 900) > 0); // across the gap
    companion_init(&c, vec2(24, 160));
    CHECK(travel(&f.lv, &c, vec2(22 * 16 + 8, 8 * 16), 900) > 0); // onto the platform
    CHECK_NEAR(c.body.pos.y + c.body.size.y, 8 * 16, 0.5f);
    fixture_free(&f);
}

static void test_a_soldier_carrying_a_crate_takes_a_route_it_can_walk(void) {
    TestMap m;
    flat_map(&m, 30);
    map_rect(&m, 10, 5, 12, 5, '#'); // a one-tile bump on the way
    Fixture f;
    CHECK(fixture_map(&f, &m));
    Companion c;
    companion_init(&c, vec2(24, 96));
    c.carrying = (EntityId){1, 1};
    CHECK(travel(&f.lv, &c, vec2(26 * 16 + 8, 96), 900) > 0);
    fixture_free(&f);
}

static void test_the_follower_recovers_when_it_is_pushed_off_its_path(void) {
    TestMap m;
    ladder_map(&m);
    Fixture f;
    CHECK(fixture_map(&f, &m));
    Companion c;
    companion_init(&c, vec2(24, 160));
    NavAgent a;
    memset(&a, 0, sizeof(a));
    InputFrame in = {0};
    const Vec2 goal = vec2(18 * 16 + 8, 5 * 16);
    bool arrived = false;
    for (i32 t = 0; t < 1500 && !arrived; t++) {
        u32 mask;
        arrived = nav_steer(&g_nav, &a, &f.lv, &c, goal, &mask) == NAV_ARRIVED;
        input_frame_update(&in, mask);
        companion_update(&c, &f.lv, &in);
        if (t == 90) {
            c.body.vel.x = -260.0f; // a blast: thrown back
            c.body.vel.y = -120.0f;
        }
    }
    CHECK(arrived);
    fixture_free(&f);
}

static void test_the_arena_is_connected_and_a_trip_across_it_works(void) {
    Arena arena;
    Level lv;
    CHECK(arena_init(&arena, 1u << 20));
    char err[128];
    CHECK(level_load_file(&lv, &arena, CS_ASSET_DIR "/levels/test_arena.lvl", err, sizeof(err)));
    nav_build(&g_nav, &lv);
    CHECK(g_nav.ok);
    // From the spawn of team 1, nearly every standing place can be reached.
    const i32 start = nav_nearest(&g_nav, lv.spawns[0]);
    u16 path[NAV_PATH_MAX];
    u32 reachable = 0;
    for (i32 i = 0; i < (i32)g_nav.node_count; i++) {
        reachable += nav_path(&g_nav, start, i, false, -1, path, NAV_PATH_MAX) >= 0 ? 1u : 0u;
    }
    CHECK(reachable * 100 >= (u32)g_nav.node_count * 95);
    // And a soldier really gets from one base to the other (a pit full of water in between).
    Companion c;
    companion_init(&c, lv.spawns[0]);
    const i32 t = travel(&lv, &c, lv.spawns[1], 3000);
    CHECK(t > 0);
    arena_free(&arena);
}

// The first real level: every standing place can be reached from both spawns, and a crate can be carried
// from where each drop zone's crates land to both pads.
static void test_the_beach_can_be_crossed_both_ways_with_a_crate(void) {
    Arena arena;
    Level lv;
    CHECK(arena_init(&arena, 1u << 20));
    char err[128];
    CHECK(level_load_file(&lv, &arena, CS_ASSET_DIR "/levels/beach.lvl", err, sizeof(err)));
    nav_build(&g_nav, &lv);
    CHECK(g_nav.ok);
    u16 path[NAV_PATH_MAX];
    for (i32 team = 0; team < 2; team++) {
        const i32 start = nav_nearest(&g_nav, lv.spawns[team]);
        for (i32 i = 0; i < (i32)g_nav.node_count; i++) {
            if (nav_path(&g_nav, start, i, false, -1, path, NAV_PATH_MAX) < 0) {
                fprintf(stderr, "  unreachable from spawn %d: %d,%d\n", team + 1, g_nav.nodes[i].tx, g_nav.nodes[i].ty);
                g_failures++;
            }
        }
    }
    for (i32 z = 0; z < lv.drop_zone_count; z++) {
        // Where the crates land: the first standing place under the drop marker.
        const i32 tx = level_px_to_tile(lv.drop_zones[z].x);
        i32 land = -1;
        for (i32 ty = level_px_to_tile(lv.drop_zones[z].y); ty < lv.height && land < 0; ty++) {
            land = nav_find(&g_nav, tx, ty, NAV_GROUND);
        }
        CHECK(land >= 0);
        for (i32 team = 0; team < 2; team++) {
            const PadSite *pad = &lv.pads[team];
            const i32 beside = nav_find(&g_nav, team == 0 ? pad->cell_x + PAD_COLS : pad->cell_x - 1, pad->cell_y, NAV_GROUND);
            CHECK(beside >= 0);
            if (land >= 0 && beside >= 0 && nav_path(&g_nav, land, beside, true, -1, path, NAV_PATH_MAX) < 0) {
                fprintf(stderr, "  no way to carry a crate from zone %d to pad %d\n", z + 1, team + 1);
                g_failures++;
            }
        }
    }
    arena_free(&arena);
}

// Random trips between reachable standing places of both levels, free and carrying a crate: all must arrive.
static u32 stress_level(const char *path, u64 seed, u32 trips, bool carrying, u32 *failed) {
    Arena arena;
    Level lv;
    char err[128];
    if (!arena_init(&arena, 1u << 20) || !level_load_file(&lv, &arena, path, err, sizeof(err))) {
        fprintf(stderr, "cannot load %s\n", path);
        g_failures++;
        return 0;
    }
    nav_build(&g_nav, &lv);
    Rng rng;
    rng_seed(&rng, seed, 3);
    u32 done = 0;
    u16 scratch[NAV_PATH_MAX];
    for (u32 t = 0; t < trips; t++) {
        const i32 a = (i32)rng_below(&rng, g_nav.node_count), b = (i32)rng_below(&rng, g_nav.node_count);
        if (g_nav.nodes[a].kind != NAV_GROUND || g_nav.nodes[b].kind != NAV_GROUND ||
            nav_path(&g_nav, a, b, carrying, -1, scratch, NAV_PATH_MAX) < 0) {
            continue;
        }
        Companion c;
        companion_init(&c, nav_node_feet(&g_nav, a));
        if (carrying) {
            c.carrying = (EntityId){1, 1};
        }
        const i32 ticks = travel(&lv, &c, nav_node_feet(&g_nav, b), 4000);
        done++;
        if (ticks < 0) {
            fprintf(stderr, "  trip %d,%d -> %d,%d failed (%d) in %s\n", g_nav.nodes[a].tx, g_nav.nodes[a].ty,
                    g_nav.nodes[b].tx, g_nav.nodes[b].ty, ticks, path);
            (*failed)++;
        }
    }
    arena_free(&arena);
    return done;
}

static void test_random_trips_all_arrive(void) {
    u32 failed = 0, done = 0;
    done += stress_level(CS_ASSET_DIR "/levels/ai_arena.lvl", 1, 60, false, &failed);
    done += stress_level(CS_ASSET_DIR "/levels/ai_arena.lvl", 2, 60, true, &failed);
    done += stress_level(CS_ASSET_DIR "/levels/test_arena.lvl", 3, 60, false, &failed);
    done += stress_level(CS_ASSET_DIR "/levels/test_arena.lvl", 4, 60, true, &failed);
    done += stress_level(CS_ASSET_DIR "/levels/beach.lvl", 5, 60, false, &failed);
    done += stress_level(CS_ASSET_DIR "/levels/beach.lvl", 6, 60, true, &failed);
    CHECK(done >= 100);
    CHECK_EQ_U64(failed, 0);
}

int main(void) {
    RUN_TEST(test_a_flat_floor_is_a_row_of_nodes_joined_by_walking);
    RUN_TEST(test_a_step_up_is_a_jump_and_a_step_down_a_walk);
    RUN_TEST(test_a_gap_is_crossed_by_a_jump_and_a_wide_one_is_not);
    RUN_TEST(test_a_ladder_joins_two_floors_but_not_for_a_crate_carrier);
    RUN_TEST(test_a_thin_platform_is_jumped_onto_and_dropped_from);
    RUN_TEST(test_a_wall_too_high_to_jump_separates_the_two_sides);
    RUN_TEST(test_a_path_is_a_chain_of_edges_that_leads_to_the_goal);
    RUN_TEST(test_the_graph_is_deterministic_and_rebuilt_alike);
    RUN_TEST(test_a_level_without_standing_place_gives_an_unusable_graph);
    RUN_TEST(test_the_follower_walks_hops_and_climbs_to_a_goal);
    RUN_TEST(test_the_follower_goes_back_down);
    RUN_TEST(test_the_follower_jumps_a_gap_and_a_thin_platform);
    RUN_TEST(test_a_soldier_carrying_a_crate_takes_a_route_it_can_walk);
    RUN_TEST(test_the_follower_recovers_when_it_is_pushed_off_its_path);
    RUN_TEST(test_the_arena_is_connected_and_a_trip_across_it_works);
    RUN_TEST(test_the_beach_can_be_crossed_both_ways_with_a_crate);
    RUN_TEST(test_random_trips_all_arrive);
    TEST_MAIN_END();
}
