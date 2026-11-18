// The AI on top of navigation: soldiers that cross floors, steal crates, flee; the team commander that builds on
// the pad and gives roles; and whole AI-vs-AI matches.
#include "test.h"

#include "app/sim.h"
#include "game/ai.h"
#include "game/combat.h"
#include "game/commander.h"
#include "game/crate.h"
#include "game/pad.h"
#include "game/squad.h"
#include "game/tuning.h"
#include "test_helpers.h"

typedef struct Env {
    Sim sim;
    InputFrame in;
} Env;

#define FLOOR_Y 192.0f // feet of a soldier on the floor of the 14-row maps (floor rows 12-13)

typedef enum MapKind {
    MAP_FLAT,   // two bases (pads at columns 8-11 and 48-51), nothing in between
    MAP_LEDGE,  // a plateau (standing row 8) with one-tile steps up on the left, a drop on the right
    MAP_LADDER, // a high block (standing row 5) with a ladder, walled in on top: up by the ladder, no way down with a crate
} MapKind;

static bool env_init(Env *e, MapKind kind) {
    TestMap m;
    map_init(&m, 60, 14);
    map_rect(&m, 0, 12, 59, 13, '#');
    map_put(&m, 1, 11, '1');
    map_put(&m, 57, 11, '2');
    map_put(&m, 8, 11, 'P');
    map_put(&m, 48, 11, 'P');
    if (kind == MAP_LEDGE) {
        map_rect(&m, 16, 11, 17, 11, '#');
        map_rect(&m, 18, 10, 19, 11, '#');
        map_rect(&m, 20, 9, 30, 11, '#');
    } else if (kind == MAP_LADDER) {
        map_rect(&m, 20, 6, 30, 11, '#');
        map_rect(&m, 19, 6, 19, 11, 'H');
        map_rect(&m, 18, 2, 18, 5, '#'); // walls too high to jump over on both sides of the top: it is a trap
        map_rect(&m, 31, 2, 31, 11, '#');
    }
    char text[16384];
    map_text(&m, text, sizeof(text));
    memset(e, 0, sizeof(*e));
    SimConfig cfg = {.level_text = text, .seed = 1};
    char err[256] = "";
    if (!sim_init(&e->sim, &cfg, err, sizeof(err))) {
        fprintf(stderr, "sim_init failed: %s\n", err);
        return false;
    }
    return true;
}

static GameState *gs_of(Env *e) {
    return &e->sim.gs;
}

static void tick_n(Env *e, int n) {
    for (int i = 0; i < n; i++) {
        input_frame_update(&e->in, 0);
        sim_tick(&e->sim, &e->in);
    }
}

static i32 spawn_at(Env *e, i32 team, SoldierVariant v, f32 feet_x, f32 feet_y) {
    return soldier_spawn(gs_of(e), team, v, vec2(feet_x, feet_y));
}

static i32 spawn(Env *e, i32 team, SoldierVariant v, f32 feet_x) {
    return spawn_at(e, team, v, feet_x, FLOOR_Y);
}

static Soldier *soldier(Env *e, i32 index) {
    return &gs_of(e)->soldiers[index];
}

static f32 cx(Env *e, i32 index) {
    return body_center(&soldier(e, index)->c.body).x;
}

static u32 pad_crates(Env *e, i32 team) {
    return pad_crate_count(gs_of(e), team);
}

static u32 products_of(Env *e, PadProduct kind) {
    u32 n = 0;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        PadProduct p;
        n += (entity_as_product(&gs_of(e)->entities.slots[i], &p) && p == kind) ? 1u : 0u;
    }
    return n;
}

static u32 soldiers_of_variant(Env *e, i32 team, SoldierVariant v) {
    u32 n = 0;
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        n += (soldier(e, i)->alive && soldier(e, i)->team == team && soldier(e, i)->variant == v) ? 1u : 0u;
    }
    return n;
}

// ---- Crates over several floors ----------------------------------------------------------------

static void test_collect_climbs_to_a_ledge_for_a_crate_and_brings_it_home(void) {
    Env e;
    CHECK(env_init(&e, MAP_LEDGE));
    spawn(&e, 0, SOLDIER_NORMAL, 100.0f);
    gs_of(&e)->teams[0].order = ORDER_COLLECT;
    crate_spawn(gs_of(&e), CRATE_RED, vec2(25 * 16.0f, 8 * 16.0f), CRATE_RESTING, -1);
    tick_n(&e, 2400);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.picked_up, 1);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.placed, 1);
    CHECK_EQ_U64(pad_crates(&e, 0), 1); // down the steps and onto the pad
    CHECK_EQ_U64(gs_of(&e)->crate_stats.thrown, 0);
    sim_free(&e.sim);
}

static void test_a_crate_that_cannot_be_carried_home_is_left_alone(void) {
    Env e;
    CHECK(env_init(&e, MAP_LADDER));
    spawn(&e, 0, SOLDIER_NORMAL, 100.0f);
    gs_of(&e)->teams[0].order = ORDER_COLLECT;
    // On top of a walled-in block the ladder leads to: a free soldier can go there, one carrying a crate cannot come back.
    crate_spawn(gs_of(&e), CRATE_RED, vec2(25 * 16.0f, 5 * 16.0f), CRATE_RESTING, -1);
    tick_n(&e, 1800);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.picked_up, 0);
    CHECK(soldier(&e, 1)->alive);
    CHECK(soldier(&e, 1)->c.body.pos.y > 8 * 16.0f); // still on the floor, not stuck up there
    sim_free(&e.sim);
}

static void test_a_stack_too_high_to_reach_is_ignored(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    spawn(&e, 0, SOLDIER_NORMAL, 330.0f);
    gs_of(&e)->teams[0].order = ORDER_COLLECT;
    for (i32 row = 11; row >= 8; row--) { // four crates high: the top one is out of reach from the floor
        crate_spawn(gs_of(&e), CRATE_RED, vec2(30 * 16.0f, (f32)(row * 16)), CRATE_RESTING, -1);
    }
    tick_n(&e, 1500);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.picked_up, 0);
    sim_free(&e.sim);
}

static void test_a_low_stack_is_taken_from_the_top(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    spawn(&e, 0, SOLDIER_NORMAL, 330.0f);
    gs_of(&e)->teams[0].order = ORDER_COLLECT;
    crate_spawn(gs_of(&e), CRATE_RED, vec2(30 * 16.0f, 11 * 16.0f), CRATE_RESTING, -1);
    crate_spawn(gs_of(&e), CRATE_RED, vec2(30 * 16.0f, 10 * 16.0f), CRATE_RESTING, -1);
    tick_n(&e, 2400);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.picked_up, 2);
    CHECK_EQ_U64(pad_crates(&e, 0), 2);
    sim_free(&e.sim);
}

// ---- Stealing ----------------------------------------------------------------------------------

static void test_steal_takes_a_crate_from_the_enemy_pad_to_ours(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    spawn(&e, 0, SOLDIER_NORMAL, 330.0f);
    CHECK(pad_debug_fill(gs_of(&e), 1, "r...", "r...")); // two stacked crates on the left edge of team 2's pad
    gs_of(&e)->teams[0].order = ORDER_STEAL;
    tick_n(&e, 3000);
    CHECK_EQ_U64(pad_crates(&e, 1), 0);
    CHECK_EQ_U64(pad_crates(&e, 0), 2);
    sim_free(&e.sim);
}

static void test_collect_leaves_the_enemy_pad_alone(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    spawn(&e, 0, SOLDIER_NORMAL, 330.0f);
    CHECK(pad_debug_fill(gs_of(&e), 1, "r...", "r..."));
    gs_of(&e)->teams[0].order = ORDER_COLLECT;
    tick_n(&e, 2000);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.picked_up, 0);
    CHECK_EQ_U64(pad_crates(&e, 1), 2);
    sim_free(&e.sim);
}

static void test_steal_does_not_take_a_crate_from_its_own_pad(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    spawn(&e, 0, SOLDIER_NORMAL, 330.0f);
    CHECK(pad_debug_fill(gs_of(&e), 0, "r...", "r..."));
    gs_of(&e)->teams[0].order = ORDER_STEAL;
    tick_n(&e, 1500);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.picked_up, 0);
    sim_free(&e.sim);
}

// ---- The commander -----------------------------------------------------------------------------

static void test_the_commander_builds_a_soldier_for_a_thin_squad(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    game_set_team_ai(gs_of(&e), 1, true);
    CHECK_EQ_U64(team_soldier_count(gs_of(&e), 1), 1); // an AI team starts with a soldier
    CHECK(pad_debug_fill(gs_of(&e), 1, "r...", "r..."));
    tick_n(&e, 120);
    CHECK_EQ_U64(team_soldier_count(gs_of(&e), 1), 2);
    CHECK_EQ_U64(gs_of(&e)->products_built, 1);
    CHECK_EQ_U64(pad_crates(&e, 1), 0);
    sim_free(&e.sim);
}

static void test_a_thin_squad_does_not_waste_crates_on_a_pistol(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    game_set_team_ai(gs_of(&e), 1, true);
    CHECK(pad_debug_fill(gs_of(&e), 1, "r...", "...."));
    tick_n(&e, 300);
    CHECK_EQ_U64(gs_of(&e)->products_built, 0); // one crate: only a pistol possible, soldiers are what it needs
    sim_free(&e.sim);
}

static void test_a_big_squad_waits_for_something_better_then_gives_in(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    game_set_team_ai(gs_of(&e), 1, true);
    spawn(&e, 1, SOLDIER_NORMAL, 900.0f);
    spawn(&e, 1, SOLDIER_NORMAL, 920.0f);
    CHECK_EQ_U64(team_soldier_count(gs_of(&e), 1), 3);
    CHECK(pad_debug_fill(gs_of(&e), 1, "r...", "r..."));
    tick_n(&e, 600);
    CHECK_EQ_U64(gs_of(&e)->products_built, 0); // an ordinary soldier is not worth it now
    tick_n(&e, 600);
    CHECK_EQ_U64(gs_of(&e)->products_built, 1); // ... but nothing else came: it builds what it can
    sim_free(&e.sim);
}

static void test_a_big_squad_builds_a_heavy_at_once(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    game_set_team_ai(gs_of(&e), 1, true);
    spawn(&e, 1, SOLDIER_NORMAL, 900.0f);
    spawn(&e, 1, SOLDIER_NORMAL, 920.0f);
    CHECK(pad_debug_fill(gs_of(&e), 1, "rr..", "rr.."));
    tick_n(&e, 90);
    CHECK_EQ_U64(soldiers_of_variant(&e, 1, SOLDIER_HEAVY), 1);
    CHECK_EQ_U64(pad_crates(&e, 1), 0);
    sim_free(&e.sim);
}

static void test_the_commander_prefers_blue_crates_products(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    game_set_team_ai(gs_of(&e), 1, true);
    CHECK(pad_debug_fill(gs_of(&e), 1, "b...", "r..."));
    tick_n(&e, 120);
    CHECK_EQ_U64(soldiers_of_variant(&e, 1, SOLDIER_STRONG), 1);
    sim_free(&e.sim);
}

static void test_the_commander_never_builds_a_vehicle(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    game_set_team_ai(gs_of(&e), 1, true);
    CHECK(pad_debug_fill(gs_of(&e), 1, "rrrr", "rrrr"));
    tick_n(&e, 400);
    CHECK(gs_of(&e)->products_built >= 1);
    CHECK_EQ_U64(products_of(&e, PRODUCT_TANK) + products_of(&e, PRODUCT_HEAVY_TANK), 0);
    CHECK(soldiers_of_variant(&e, 1, SOLDIER_HEAVY) >= 1); // the best a full pad allows
    sim_free(&e.sim);
}

static void test_the_commander_does_not_build_past_its_squad_limit(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    game_set_team_ai(gs_of(&e), 1, true);
    for (u32 i = 1; i < TUNING_COMMANDER_MAX_SQUAD; i++) {
        spawn(&e, 1, SOLDIER_NORMAL, 700.0f + (f32)i * 14.0f);
    }
    CHECK_EQ_U64(team_soldier_count(gs_of(&e), 1), TUNING_COMMANDER_MAX_SQUAD);
    CHECK(pad_debug_fill(gs_of(&e), 1, "rr..", "rr.."));
    tick_n(&e, 1200);
    CHECK_EQ_U64(team_soldier_count(gs_of(&e), 1), TUNING_COMMANDER_MAX_SQUAD);
    sim_free(&e.sim);
}

static void test_the_commander_hands_out_roles(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    game_set_team_ai(gs_of(&e), 1, true);
    const i32 heavy = spawn(&e, 1, SOLDIER_HEAVY, 700.0f);
    const i32 a = spawn(&e, 1, SOLDIER_NORMAL, 720.0f);
    const i32 b = spawn(&e, 1, SOLDIER_NORMAL, 740.0f);
    crate_spawn(gs_of(&e), CRATE_RED, vec2(30 * 16.0f, 11 * 16.0f), CRATE_RESTING, -1);
    tick_n(&e, 40);
    CHECK(soldier(&e, heavy)->ai.role == ORDER_ATTACK);
    const u32 collectors = (soldier(&e, a)->ai.role == ORDER_COLLECT ? 1u : 0u) +
                           (soldier(&e, b)->ai.role == ORDER_COLLECT ? 1u : 0u) +
                           (soldier(&e, 1)->ai.role == ORDER_COLLECT ? 1u : 0u);
    CHECK(collectors >= 1 && collectors <= 3);
    sim_free(&e.sim);
}

static void test_a_team_under_threat_defends_its_base(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    game_set_team_ai(gs_of(&e), 1, true);
    const i32 heavy = spawn(&e, 1, SOLDIER_HEAVY, 700.0f);
    spawn(&e, 1, SOLDIER_NORMAL, 720.0f);
    spawn(&e, 1, SOLDIER_NORMAL, 740.0f);
    crate_spawn(gs_of(&e), CRATE_RED, vec2(30 * 16.0f, 11 * 16.0f), CRATE_RESTING, -1);
    spawn(&e, 0, SOLDIER_NORMAL, 600.0f); // an enemy 170 px from the pad of team 2 (it stands at x = 768)
    soldier(&e, 0)->c.hp = 100;
    gs_of(&e)->teams[0].order = ORDER_GUARD;
    tick_n(&e, 40);
    CHECK(soldier(&e, heavy)->ai.role == ORDER_GUARD); // not the offensive
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        if (soldier(&e, i)->alive && soldier(&e, i)->team == 1) {
            CHECK(soldier(&e, i)->ai.role != ORDER_ATTACK && soldier(&e, i)->ai.role != ORDER_STEAL);
        }
    }
    sim_free(&e.sim);
}

static void test_a_human_team_has_no_commander(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    CHECK(pad_debug_fill(gs_of(&e), 0, "r...", "r..."));
    tick_n(&e, 400);
    CHECK_EQ_U64(gs_of(&e)->products_built, 0); // only the player (menu) builds for the human's team
    sim_free(&e.sim);
}

static void test_an_ai_team_zero_leaves_no_soldier_to_the_human(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    game_set_team_ai(gs_of(&e), 0, true);
    CHECK(gs_of(&e)->controlled < 0);
    CHECK_EQ_U64(team_soldier_count(gs_of(&e), 0), 1); // still there, now played by the AI
    CHECK(pad_debug_fill(gs_of(&e), 0, "r...", "r..."));
    tick_n(&e, 120);
    CHECK_EQ_U64(team_soldier_count(gs_of(&e), 0), 2);
    CHECK(gs_of(&e)->controlled < 0); // the new soldier is not handed to a human either
    sim_free(&e.sim);
}

// ---- Fighting and fleeing ----------------------------------------------------------------------

static void test_a_hurt_soldier_runs_for_its_base(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 500.0f);
    const i32 foe = spawn(&e, 1, SOLDIER_NORMAL, 560.0f);
    gs_of(&e)->teams[0].order = ORDER_GUARD;
    soldier(&e, a)->c.hp = 20; // below a third
    soldier(&e, foe)->ai.role = ORDER_GUARD;
    const f32 x0 = cx(&e, a);
    tick_n(&e, 6);
    CHECK(soldier(&e, a)->ai.mode == AI_FLEE);
    tick_n(&e, 40);
    CHECK(cx(&e, a) < x0 - 20.0f); // toward the pad at the left
    sim_free(&e.sim);
}

static void test_a_healthy_soldier_does_not_flee(void) {
    Env e;
    CHECK(env_init(&e, MAP_FLAT));
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 500.0f);
    spawn(&e, 1, SOLDIER_NORMAL, 560.0f);
    gs_of(&e)->teams[0].order = ORDER_GUARD;
    tick_n(&e, 6);
    CHECK(soldier(&e, a)->ai.mode != AI_FLEE);
    sim_free(&e.sim);
}

static void test_follow_climbs_after_the_leader(void) {
    Env e;
    CHECK(env_init(&e, MAP_LEDGE));
    const i32 buddy = spawn(&e, 0, SOLDIER_NORMAL, 100.0f);
    gs_of(&e)->teams[0].order = ORDER_FOLLOW;
    // The leader (soldier 0, controlled, standing still) is on the plateau.
    companion_init(&soldier(&e, 0)->c, vec2(26 * 16.0f, 9 * 16.0f));
    tick_n(&e, 900);
    CHECK_NEAR(soldier(&e, buddy)->c.body.pos.y + soldier(&e, buddy)->c.body.size.y, 9 * 16.0f, 1.0f); // up there
    CHECK(fabsf(cx(&e, buddy) - cx(&e, 0)) <= TUNING_AI_FOLLOW_BAND + 4.0f);
    sim_free(&e.sim);
}

static void test_attack_crosses_the_level_to_the_enemy_pad(void) {
    Env e;
    CHECK(env_init(&e, MAP_LEDGE));
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 100.0f);
    gs_of(&e)->teams[0].order = ORDER_ATTACK;
    tick_n(&e, 1500);
    CHECK(fabsf(cx(&e, a) - 800.0f) <= 60.0f); // over the plateau, then down to the pad at x = 800
    sim_free(&e.sim);
}

// ---- Whole matches -----------------------------------------------------------------------------

static void check_match_invariants(const Sim *sim) {
    const GameState *gs = &sim->gs;
    const Level *lv = &sim->level;
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        const Soldier *s = &gs->soldiers[i];
        if (!s->alive) {
            continue;
        }
        const Companion *c = &s->c;
        CHECK(s->team >= 0 && s->team < MAX_TEAMS && gs->teams[s->team].active);
        CHECK(c->hp > 0 && c->hp <= c->max_hp);
        CHECK(c->body.pos.x >= 0.0f && c->body.pos.x + c->body.size.x <= level_width_px(lv));
        const Vec2 ctr = body_center(&c->body);
        CHECK(!level_is_solid(lv, level_px_to_tile(ctr.x), level_px_to_tile(ctr.y)));
        CHECK(s->ai.role >= 0 && s->ai.role < ORDER_COUNT && s->ai.mode >= 0 && s->ai.mode < AI_MODE_COUNT);
        CHECK(s->ai.nav.path_len <= NAV_PATH_MAX && s->ai.nav.path_pos <= s->ai.nav.path_len);
    }
}

static bool match_init(Sim *sim, u64 seed, u32 ai_mask) {
    SimConfig cfg = {.level_path = CS_ASSET_DIR "/levels/ai_arena.lvl", .seed = seed, .ai_teams = ai_mask};
    char err[256] = "";
    if (!sim_init(sim, &cfg, err, sizeof(err))) {
        fprintf(stderr, "sim_init failed: %s\n", err);
        return false;
    }
    return true;
}

static void test_two_ai_teams_play_a_coherent_match(void) {
    static Sim sim;
    CHECK(match_init(&sim, 1, 3u));
    CHECK(sim.gs.controlled < 0);
    CHECK(team_soldier_count(&sim.gs, 0) == 1 && team_soldier_count(&sim.gs, 1) == 1);
    u32 max_squad[2] = {1, 1};
    for (int t = 0; t < 7000; t++) {
        sim_tick(&sim, NULL);
        if (t % 50 == 0) {
            check_match_invariants(&sim);
        }
        for (i32 team = 0; team < 2; team++) {
            max_squad[team] = MAX(max_squad[team], team_soldier_count(&sim.gs, team));
        }
    }
    const GameState *gs = &sim.gs;
    CHECK(gs->nav.ok); // the AI built its graph
    CHECK(gs->crate_stats.picked_up >= 4 && gs->crate_stats.placed >= 3); // they gather and stack crates
    CHECK(gs->products_built >= 3);                                        // ... and build with them
    CHECK(max_squad[0] >= 2 && max_squad[1] >= 2);                         // both teams grew
    CHECK_EQ_U64(gs->crate_stats.thrown, 0);                               // nobody threw a crate away
    CHECK(team_soldier_count(gs, 0) + team_soldier_count(gs, 1) >= 1);
    sim_free(&sim);
}

static u64 run_match(u64 seed, u32 ticks, bool armed) {
    static Sim sim;
    CHECK(match_init(&sim, seed, 3u));
    if (armed) { // a heavy each, so that the two sides meet with something to shoot with
        soldier_spawn(&sim.gs, 0, SOLDIER_HEAVY, vec2(100.0f, 432.0f));
        soldier_spawn(&sim.gs, 1, SOLDIER_HEAVY, vec2(1180.0f, 432.0f));
    }
    for (u32 t = 0; t < ticks; t++) {
        sim_tick(&sim, NULL);
    }
    const u64 hash = game_hash(&sim.gs);
    sim_free(&sim);
    return hash;
}

static void test_a_match_is_deterministic(void) {
    CHECK(run_match(1, 3000, true) == run_match(1, 3000, true));
    CHECK(run_match(1, 3000, true) != run_match(1, 3001, true));
}

static void test_the_heavies_of_two_ai_teams_fight(void) {
    static Sim sim;
    CHECK(match_init(&sim, 1, 3u));
    soldier_spawn(&sim.gs, 0, SOLDIER_HEAVY, vec2(100.0f, 432.0f));
    soldier_spawn(&sim.gs, 1, SOLDIER_HEAVY, vec2(1180.0f, 432.0f));
    for (int t = 0; t < 6000; t++) {
        sim_tick(&sim, NULL);
        if (t % 50 == 0) {
            check_match_invariants(&sim);
        }
    }
    CHECK(sim.gs.combat_stats.shots > 0 && sim.gs.combat_stats.hits > 0); // they met and fought
    CHECK(sim.gs.combat_stats.explosions > 0);                            // with rockets
    sim_free(&sim);
}

static void test_the_default_opponents_are_ai_teams_with_a_pad(void) {
    Sim sim;
    SimConfig cfg = {.level_path = CS_ASSET_DIR "/levels/ai_arena.lvl", .seed = 1, .ai_opponents = true};
    char err[256] = "";
    CHECK(sim_init(&sim, &cfg, err, sizeof(err)));
    CHECK(!sim.gs.teams[0].ai && sim.gs.teams[1].ai);
    CHECK(!sim.gs.teams[2].ai && !sim.gs.teams[3].ai); // no spawn, no pad
    CHECK(sim.gs.controlled == 0); // the human keeps their soldier
    CHECK_EQ_U64(team_soldier_count(&sim.gs, 1), 1);
    sim_free(&sim);
}

int main(void) {
    RUN_TEST(test_collect_climbs_to_a_ledge_for_a_crate_and_brings_it_home);
    RUN_TEST(test_a_crate_that_cannot_be_carried_home_is_left_alone);
    RUN_TEST(test_a_stack_too_high_to_reach_is_ignored);
    RUN_TEST(test_a_low_stack_is_taken_from_the_top);
    RUN_TEST(test_steal_takes_a_crate_from_the_enemy_pad_to_ours);
    RUN_TEST(test_collect_leaves_the_enemy_pad_alone);
    RUN_TEST(test_steal_does_not_take_a_crate_from_its_own_pad);
    RUN_TEST(test_the_commander_builds_a_soldier_for_a_thin_squad);
    RUN_TEST(test_a_thin_squad_does_not_waste_crates_on_a_pistol);
    RUN_TEST(test_a_big_squad_waits_for_something_better_then_gives_in);
    RUN_TEST(test_a_big_squad_builds_a_heavy_at_once);
    RUN_TEST(test_the_commander_prefers_blue_crates_products);
    RUN_TEST(test_the_commander_never_builds_a_vehicle);
    RUN_TEST(test_the_commander_does_not_build_past_its_squad_limit);
    RUN_TEST(test_the_commander_hands_out_roles);
    RUN_TEST(test_a_team_under_threat_defends_its_base);
    RUN_TEST(test_a_human_team_has_no_commander);
    RUN_TEST(test_an_ai_team_zero_leaves_no_soldier_to_the_human);
    RUN_TEST(test_a_hurt_soldier_runs_for_its_base);
    RUN_TEST(test_a_healthy_soldier_does_not_flee);
    RUN_TEST(test_follow_climbs_after_the_leader);
    RUN_TEST(test_attack_crosses_the_level_to_the_enemy_pad);
    RUN_TEST(test_two_ai_teams_play_a_coherent_match);
    RUN_TEST(test_a_match_is_deterministic);
    RUN_TEST(test_the_heavies_of_two_ai_teams_fight);
    RUN_TEST(test_the_default_opponents_are_ai_teams_with_a_pad);
    TEST_MAIN_END();
}
