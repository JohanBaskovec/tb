// Squads: soldier variants, teams, control switching, orders, and the AI that carries them out
// (following, guarding, shooting, picking weapons up, fetching crates to the pad, attacking).
#include "test.h"

#include "app/sim.h"
#include "game/ai.h"
#include "game/base.h"
#include "game/combat.h"
#include "game/crate.h"
#include "game/loot.h"
#include "game/pad.h"
#include "game/squad.h"
#include "game/tuning.h"
#include "test_helpers.h"

// ---- Environment ---------------------------------------------------------------------------------

typedef struct Env {
    Sim sim;
    InputFrame in;
} Env;

#define FLOOR_Y 160.0f

// 60x12 map, floor rows 10-11. Team 1 spawns at column 1 with its pad at columns 8-11 (marker on the
// bottom-left cell); team 2 spawns at column 57 with its pad at columns 48-51. A wall (column 30, rows 6-9)
// separates the two halves when `wall` is set.
static bool env_init(Env *e, bool wall) {
    TestMap m;
    map_init(&m, 60, 12);
    map_rect(&m, 0, 10, 59, 11, '#');
    map_put(&m, 1, 9, '1');
    map_put(&m, 57, 9, '2');
    map_put(&m, 8, 9, 'P');
    map_put(&m, 48, 9, 'P');
    if (wall) {
        map_rect(&m, 30, 6, 30, 9, '#');
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

static void tick(Env *e, u32 mask) {
    input_frame_update(&e->in, mask);
    sim_tick(&e->sim, &e->in);
}

static void tick_n(Env *e, int n) {
    for (int i = 0; i < n; i++) {
        tick(e, 0);
    }
}

static i32 spawn(Env *e, i32 team, SoldierVariant v, f32 feet_x) {
    return soldier_spawn(gs_of(e), team, v, vec2(feet_x, FLOOR_Y));
}

static Companion *soldier(Env *e, i32 index) {
    return &gs_of(e)->soldiers[index].c;
}

static f32 cx(Env *e, i32 index) {
    return body_center(&soldier(e, index)->body).x;
}

static void move_soldier(Env *e, i32 index, f32 feet_x) {
    const Soldier keep = gs_of(e)->soldiers[index];
    companion_init(&gs_of(e)->soldiers[index].c, vec2(feet_x, FLOOR_Y));
    gs_of(e)->soldiers[index].c.hp = keep.c.hp;
    gs_of(e)->soldiers[index].c.max_hp = keep.c.max_hp;
}

static u32 count_type(Env *e, EntityType type) {
    u32 n = 0;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        n += gs_of(e)->entities.slots[i].type == type ? 1u : 0u;
    }
    return n;
}

static u32 pad_crates(Env *e, i32 team) {
    PadGrid g;
    pad_read_grid(gs_of(e), team, &g, NULL);
    u32 n = 0;
    for (int r = 0; r < PAD_ROWS; r++) {
        for (int c = 0; c < PAD_COLS; c++) {
            n += g.cells[r][c] != PAD_EMPTY ? 1u : 0u;
        }
    }
    return n;
}

// ---- Variants ------------------------------------------------------------------------------------

static void test_variants_hit_points_and_equipment(void) {
    CHECK_EQ_U64(soldier_def(SOLDIER_NORMAL)->hp, 100);
    CHECK_EQ_U64(soldier_def(SOLDIER_STRONG)->hp, 150);
    CHECK_EQ_U64(soldier_def(SOLDIER_HEAVY)->hp, 200);
    CHECK_EQ_U64(soldier_def(SOLDIER_HEAVY_ARMORED)->hp, 300);
    CHECK(soldier_def(SOLDIER_NORMAL)->start_weapon == WEAPON_FISTS);
    CHECK(soldier_def(SOLDIER_STRONG)->start_weapon == WEAPON_FISTS);
    CHECK(soldier_def(SOLDIER_HEAVY)->start_weapon == WEAPON_ROCKET_LAUNCHER);
    CHECK(soldier_def(SOLDIER_HEAVY_ARMORED)->start_weapon == WEAPON_ROCKET_LAUNCHER);
    // Strictly stronger from one to the next.
    for (int v = 1; v < SOLDIER_VARIANT_COUNT; v++) {
        CHECK(soldier_def((SoldierVariant)v)->hp > soldier_def((SoldierVariant)(v - 1))->hp);
    }
    CHECK(soldier_def((SoldierVariant)99) == soldier_def(SOLDIER_NORMAL)); // out of range: the base soldier
}

static void test_products_map_to_variants(void) {
    SoldierVariant v;
    CHECK(soldier_from_product(PRODUCT_COMPANION, &v) && v == SOLDIER_NORMAL);
    CHECK(soldier_from_product(PRODUCT_STRONG_COMPANION, &v) && v == SOLDIER_STRONG);
    CHECK(soldier_from_product(PRODUCT_HEAVY_COMPANION, &v) && v == SOLDIER_HEAVY);
    CHECK(soldier_from_product(PRODUCT_HEAVY_ARMORED_COMPANION, &v) && v == SOLDIER_HEAVY_ARMORED);
    CHECK(!soldier_from_product(PRODUCT_PISTOL, &v));
    CHECK(!soldier_from_product(PRODUCT_TANK, &v));
}

static void test_the_pad_makes_every_variant(void) {
    struct {
        PadShape shape;
        bool blue;
        const char *rows[2];
        SoldierVariant expect;
    } cases[] = {
        {SHAPE_2X1_V, false, {"r...", "r..."}, SOLDIER_NORMAL},
        {SHAPE_2X1_V, true, {"r...", "b..."}, SOLDIER_STRONG},
        {SHAPE_2X2, false, {"rr..", "rr.."}, SOLDIER_HEAVY},
        {SHAPE_2X2, true, {"rr..", "rb.."}, SOLDIER_HEAVY_ARMORED},
    };
    for (u32 i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        Env e;
        CHECK(env_init(&e, false));
        CHECK(pad_debug_fill(gs_of(&e), 0, cases[i].rows[0], cases[i].rows[1]));
        CHECK(pad_build(gs_of(&e), 0, cases[i].shape, cases[i].blue));
        CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_SOLDIER_SPAWNED), 1);
        tick_n(&e, 60);
        const Soldier *s = &gs_of(&e)->soldiers[1];
        CHECK(s->alive && s->team == 0 && s->variant == cases[i].expect);
        CHECK_EQ_U64(s->c.hp, soldier_def(cases[i].expect)->hp);
        CHECK_EQ_U64(s->c.max_hp, soldier_def(cases[i].expect)->hp);
        CHECK(s->c.weapon == soldier_def(cases[i].expect)->start_weapon);
        if (s->c.weapon != WEAPON_FISTS) {
            CHECK_EQ_U64(s->c.ammo, weapon_def(s->c.weapon)->max_ammo);
        }
        CHECK(s->c.body.on_ground);
        CHECK(s->c.body.pos.x + s->c.body.size.x <= 8 * 16); // left of the pad (columns 8-11)
        CHECK_EQ_U64(count_type(&e, ENT_PRODUCT), 0);
        sim_free(&e.sim);
    }
}

static void test_a_pad_without_room_builds_nothing(void) {
    Env e;
    CHECK(env_init(&e, false));
    CHECK(pad_debug_fill(gs_of(&e), 0, "r...", "r..."));
    // Fill the squad table: the next build is refused and consumes nothing.
    while (spawn(&e, 0, SOLDIER_NORMAL, 300.0f) >= 0) {
    }
    CHECK(!pad_build(gs_of(&e), 0, SHAPE_2X1_V, false));
    CHECK_EQ_U64(gs_of(&e)->crate_stats.consumed, 0);
    CHECK_EQ_U64(gs_of(&e)->products_built, 0);
    sim_free(&e.sim);
}

static void test_spawn_checks_the_room_and_the_arguments(void) {
    Env e;
    CHECK(env_init(&e, true));
    CHECK(soldier_spawn(gs_of(&e), 0, SOLDIER_NORMAL, vec2(30 * 16 + 8, FLOOR_Y)) < 0); // inside the wall
    CHECK(soldier_spawn(gs_of(&e), MAX_TEAMS, SOLDIER_NORMAL, vec2(300, FLOOR_Y)) < 0);
    CHECK(soldier_spawn(gs_of(&e), -1, SOLDIER_NORMAL, vec2(300, FLOOR_Y)) < 0);
    CHECK(soldier_spawn(gs_of(&e), 0, SOLDIER_VARIANT_COUNT, vec2(300, FLOOR_Y)) < 0);
    CHECK_EQ_U64(team_soldier_count(gs_of(&e), 0), 1);
    const i32 i = spawn(&e, 1, SOLDIER_HEAVY, 400.0f);
    CHECK(i == 1 && gs_of(&e)->soldiers[i].team == 1);
    CHECK_EQ_U64(team_soldier_count(gs_of(&e), 1), 1);
    CHECK(gs_of(&e)->controlled == 0); // an enemy soldier is not the human's
    sim_free(&e.sim);
}

// ---- Teams and control ---------------------------------------------------------------------------

static void test_the_game_starts_with_one_soldier_for_team_one(void) {
    Env e;
    CHECK(env_init(&e, false));
    CHECK(gs_of(&e)->teams[0].active && gs_of(&e)->teams[1].active && !gs_of(&e)->teams[2].active);
    CHECK_EQ_U64(team_soldier_count(gs_of(&e), 0), 1);
    CHECK_EQ_U64(team_soldier_count(gs_of(&e), 1), 0);
    CHECK(gs_of(&e)->controlled == 0);
    CHECK(gs_of(&e)->teams[0].order == ORDER_FOLLOW && gs_of(&e)->teams[1].order == ORDER_GUARD);
    CHECK(!gs_of(&e)->teams[1].eliminated); // never had a soldier: not "eliminated"
    sim_free(&e.sim);
}

static void test_switch_cycles_through_the_team(void) {
    Env e;
    CHECK(env_init(&e, false));
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 100.0f);
    const i32 foe = spawn(&e, 1, SOLDIER_NORMAL, 200.0f);
    const i32 b = spawn(&e, 0, SOLDIER_STRONG, 300.0f);
    CHECK(a == 1 && foe == 2 && b == 3);
    CHECK(gs_of(&e)->controlled == 0);
    tick(&e, KEY(ACT_SWITCH));
    CHECK(gs_of(&e)->controlled == 1);
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_CONTROL_CHANGED), 1);
    tick(&e, 0);
    tick(&e, KEY(ACT_SWITCH));
    CHECK(gs_of(&e)->controlled == 3); // the enemy is skipped
    tick(&e, 0);
    tick(&e, KEY(ACT_SWITCH));
    CHECK(gs_of(&e)->controlled == 0); // wraps around
    // Holding the key does not keep switching.
    tick(&e, 0);
    for (int i = 0; i < 10; i++) {
        tick(&e, KEY(ACT_SWITCH));
    }
    CHECK(gs_of(&e)->controlled == 1);
    sim_free(&e.sim);
}

static void test_switch_with_one_soldier_changes_nothing(void) {
    Env e;
    CHECK(env_init(&e, false));
    tick(&e, KEY(ACT_SWITCH));
    CHECK(gs_of(&e)->controlled == 0);
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_CONTROL_CHANGED), 0);
    sim_free(&e.sim);
}

static void test_tab_belongs_to_the_pad_menu_while_it_is_up(void) {
    Env e;
    CHECK(env_init(&e, false));
    spawn(&e, 0, SOLDIER_NORMAL, 100.0f);
    CHECK(pad_debug_fill(gs_of(&e), 0, "....", "rr.."));
    move_soldier(&e, 0, 150.0f); // next to the pad: the menu is up
    tick_n(&e, 30);
    CHECK(e.sim.menu.visible);
    tick(&e, KEY(ACT_SWITCH));
    CHECK(gs_of(&e)->controlled == 0); // not switched
    CHECK_EQ_U64(e.sim.menu.selected, 1); // the menu moved
    move_soldier(&e, 0, 400.0f);
    tick_n(&e, 5);
    CHECK(!e.sim.menu.visible);
    tick(&e, KEY(ACT_SWITCH));
    CHECK(gs_of(&e)->controlled == 1); // away from the pad, Tab switches
    sim_free(&e.sim);
}

static void test_the_controlled_soldier_is_the_one_that_obeys_the_keys(void) {
    Env e;
    CHECK(env_init(&e, false));
    spawn(&e, 0, SOLDIER_NORMAL, 100.0f);
    gs_of(&e)->teams[0].order = ORDER_GUARD;
    tick_n(&e, 5);
    const f32 x0 = cx(&e, 0), x1 = cx(&e, 1);
    for (int i = 0; i < 30; i++) {
        tick(&e, KEY(ACT_RIGHT));
    }
    CHECK(cx(&e, 0) > x0 + 20.0f);          // soldier 0 walked
    CHECK(fabsf(cx(&e, 1) - x1) < 60.0f);   // soldier 1 follows its own AI, not the keys
    tick(&e, KEY(ACT_SWITCH));
    tick(&e, 0);
    const f32 y0 = cx(&e, 0), y1 = cx(&e, 1);
    for (int i = 0; i < 30; i++) {
        tick(&e, KEY(ACT_RIGHT));
    }
    CHECK(cx(&e, 1) > y1 + 20.0f);
    CHECK(fabsf(cx(&e, 0) - y0) < 100.0f); // the other one is now AI-driven
    sim_free(&e.sim);
}

static void test_death_passes_control_to_the_closest_ally(void) {
    Env e;
    CHECK(env_init(&e, false));
    spawn(&e, 0, SOLDIER_NORMAL, 400.0f);  // slot 1, far
    spawn(&e, 0, SOLDIER_NORMAL, 60.0f);   // slot 2, close to the controlled one at x=24
    spawn(&e, 1, SOLDIER_NORMAL, 40.0f);   // an enemy right next to it: not an ally
    soldier(&e, 0)->hp = 0;
    tick(&e, 0);
    CHECK(!gs_of(&e)->soldiers[0].alive);
    CHECK(gs_of(&e)->controlled == 2);
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_SOLDIER_DIED), 1);
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_CONTROL_CHANGED), 1);
    CHECK(!gs_of(&e)->teams[0].eliminated);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.deaths, 1);
    // The new controlled soldier answers the keys.
    const f32 x = cx(&e, 2);
    for (int i = 0; i < 20; i++) {
        tick(&e, KEY(ACT_RIGHT));
    }
    CHECK(cx(&e, 2) > x + 10.0f);
    sim_free(&e.sim);
}

static void test_a_team_is_eliminated_with_its_last_soldier(void) {
    Env e;
    CHECK(env_init(&e, false));
    const i32 b = spawn(&e, 0, SOLDIER_NORMAL, 400.0f);
    const i32 foe = spawn(&e, 1, SOLDIER_NORMAL, 700.0f);
    soldier(&e, 0)->hp = 0;
    tick(&e, 0);
    CHECK(gs_of(&e)->controlled == b && !gs_of(&e)->teams[0].eliminated);
    soldier(&e, b)->hp = 0;
    tick(&e, 0);
    CHECK(gs_of(&e)->controlled == -1 && gs_of(&e)->teams[0].eliminated);
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_TEAM_ELIMINATED), 1);
    CHECK(!gs_of(&e)->teams[1].eliminated && gs_of(&e)->soldiers[foe].alive);
    tick_n(&e, 60);
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_TEAM_ELIMINATED), 0); // announced once
    // Keys do nothing, nothing crashes; the ghost is what the camera and HUD read.
    tick(&e, KEY(ACT_SWITCH) | KEY(ACT_ORDER) | KEY(ACT_FIRE) | KEY(ACT_GRAB));
    CHECK(game_player(gs_of(&e))->hp <= 0);
    // A soldier made later (a pad that survived) brings the team back, and the human controls it.
    const i32 c = spawn(&e, 0, SOLDIER_NORMAL, 500.0f);
    CHECK(c >= 0 && !gs_of(&e)->teams[0].eliminated && gs_of(&e)->controlled == c);
    sim_free(&e.sim);
}

static void test_water_sends_any_soldier_back_to_its_spawn(void) {
    TestMap m;
    map_init(&m, 30, 12);
    map_rect(&m, 0, 10, 29, 11, '#');
    map_rect(&m, 14, 10, 17, 11, '~');
    map_put(&m, 1, 9, '1');
    map_put(&m, 27, 9, '2');
    char text[8192];
    map_text(&m, text, sizeof(text));
    Sim sim;
    SimConfig cfg = {.level_text = text, .seed = 1};
    char err[128];
    CHECK(sim_init(&sim, &cfg, err, sizeof(err)));
    const i32 h = soldier_spawn(&sim.gs, 1, SOLDIER_HEAVY, vec2(15 * 16.0f, 140.0f));
    CHECK(h >= 0);
    sim.gs.soldiers[h].c.ammo = 2;
    InputFrame in = {0};
    for (int i = 0; i < 80; i++) {
        input_frame_update(&in, 0);
        sim_tick(&sim, &in);
    }
    CHECK_EQ_U64(sim.gs.respawns, 1);
    CHECK(sim.gs.soldiers[h].alive && sim.gs.soldiers[h].variant == SOLDIER_HEAVY);
    CHECK(body_center(&sim.gs.soldiers[h].c.body).x > 20 * 16); // team 2's spawn, column 27
    CHECK_EQ_U64(sim.gs.soldiers[h].c.ammo, weapon_def(WEAPON_ROCKET_LAUNCHER)->max_ammo); // equipped anew
    CHECK_EQ_U64(sim.gs.combat_stats.deaths, 0);
    sim_free(&sim);
}

// ---- Orders --------------------------------------------------------------------------------------

static void test_the_order_key_cycles_the_team_order(void) {
    Env e;
    CHECK(env_init(&e, false));
    CHECK(gs_of(&e)->teams[0].order == ORDER_FOLLOW);
    const Order expected[] = {ORDER_GUARD, ORDER_COLLECT, ORDER_ATTACK, ORDER_STEAL, ORDER_FOLLOW};
    for (int i = 0; i < 5; i++) {
        tick(&e, KEY(ACT_ORDER));
        CHECK(gs_of(&e)->teams[0].order == expected[i]);
        CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_ORDER_CHANGED), 1);
        tick(&e, 0);
    }
    for (int i = 0; i < 10; i++) { // holding it changes once
        tick(&e, KEY(ACT_ORDER));
    }
    CHECK(gs_of(&e)->teams[0].order == ORDER_GUARD);
    CHECK(gs_of(&e)->teams[1].order == ORDER_GUARD); // only the controlled soldier's team
    CHECK(strcmp(order_name(ORDER_ATTACK), "attack") == 0 && strcmp(order_name(ORDER_COUNT), "?") == 0);
    sim_free(&e.sim);
}

static void test_follow_keeps_close_to_the_controlled_soldier(void) {
    Env e;
    CHECK(env_init(&e, false));
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 500.0f);
    move_soldier(&e, 0, 200.0f);
    tick_n(&e, 400);
    CHECK(fabsf(cx(&e, a) - cx(&e, 0)) <= TUNING_AI_FOLLOW_BAND + 6.0f);
    // The leader walks away; the follower comes along.
    for (int i = 0; i < 180; i++) {
        tick(&e, KEY(ACT_RIGHT));
    }
    tick_n(&e, 200);
    CHECK(cx(&e, 0) > 300.0f);
    CHECK(fabsf(cx(&e, a) - cx(&e, 0)) <= TUNING_AI_FOLLOW_BAND + 6.0f);
    sim_free(&e.sim);
}

static void test_guard_stays_near_the_base(void) {
    Env e;
    CHECK(env_init(&e, false));
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 600.0f);
    gs_of(&e)->teams[0].order = ORDER_GUARD;
    move_soldier(&e, 0, 800.0f); // the leader is elsewhere: guarding does not follow it
    tick_n(&e, 700);
    const f32 home = gs_of(&e)->teams[0].spawn.x; // in front of the base, off the pad (crates must settle there)
    CHECK(fabsf(cx(&e, a) - home) <= TUNING_AI_GUARD_BAND + 6.0f);
    const Rect pad = pad_rect(gs_of(&e), 0);
    const Rect body = body_rect(&soldier(&e, a)->body);
    CHECK(body.x + body.w <= pad.x || body.x >= pad.x + pad.w);
    sim_free(&e.sim);
}

static void test_attack_goes_to_the_enemy_base_and_hits_it(void) {
    Env e;
    CHECK(env_init(&e, false));
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 200.0f);
    gs_of(&e)->teams[0].order = ORDER_ATTACK;
    tick_n(&e, 900);
    const Rect base = base_rect(gs_of(&e), 1);
    CHECK(cx(&e, a) >= base.x - 4.0f * TILE_SIZE && cx(&e, a) <= base.x + base.w); // at the building, from the left
    CHECK(gs_of(&e)->teams[1].base_hp < TUNING_BASE_HP); // punching it
    CHECK(gs_of(&e)->teams[0].stats.base_damage == (u32)(TUNING_BASE_HP - gs_of(&e)->teams[1].base_hp));
    sim_free(&e.sim);
}

// ---- AI: fighting --------------------------------------------------------------------------------

static void test_ai_soldiers_shoot_enemies_in_range(void) {
    Env e;
    CHECK(env_init(&e, false));
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 330.0f);
    soldier_give_weapon(soldier(&e, a), WEAPON_PISTOL);
    move_soldier(&e, 0, 300.0f); // the leader next to it: the follower stays put
    dummy_spawn(gs_of(&e), vec2(440.0f, FLOOR_Y), TEAM_NONE);
    tick_n(&e, 400);
    CHECK(gs_of(&e)->combat_stats.shots > 3);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.kills, 1); // a pistol, 12 damage a bullet: the dummy is gone
    CHECK(soldier(&e, a)->ammo < weapon_def(WEAPON_PISTOL)->max_ammo);
    sim_free(&e.sim);
}

static void test_ai_does_not_shoot_through_walls(void) {
    Env e;
    CHECK(env_init(&e, true)); // wall at x 480..496
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 430.0f);
    soldier_give_weapon(soldier(&e, a), WEAPON_PISTOL);
    move_soldier(&e, 0, 400.0f);
    dummy_spawn(gs_of(&e), vec2(540.0f, FLOOR_Y), TEAM_NONE);
    tick_n(&e, 200);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 0);
    sim_free(&e.sim);
}

static void test_ai_does_not_shoot_out_of_range(void) {
    Env e;
    CHECK(env_init(&e, false));
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 330.0f);
    soldier_give_weapon(soldier(&e, a), WEAPON_PISTOL);
    move_soldier(&e, 0, 300.0f);
    dummy_spawn(gs_of(&e), vec2(330.0f + TUNING_AI_RANGE + 60.0f, FLOOR_Y), TEAM_NONE);
    tick_n(&e, 150);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 0);
    sim_free(&e.sim);
}

static void test_soldiers_of_two_teams_fight_each_other(void) {
    Env e;
    CHECK(env_init(&e, false));
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 330.0f);
    const i32 b = spawn(&e, 1, SOLDIER_NORMAL, 430.0f);
    soldier_give_weapon(soldier(&e, a), WEAPON_UZI);
    soldier_give_weapon(soldier(&e, b), WEAPON_UZI);
    move_soldier(&e, 0, 300.0f);
    gs_of(&e)->teams[1].order = ORDER_GUARD; // its home is far away; it still shoots what it sees
    tick_n(&e, 200);
    CHECK(soldier(&e, a)->hp < soldier(&e, a)->max_hp || !gs_of(&e)->soldiers[a].alive);
    CHECK(soldier(&e, b)->hp < soldier(&e, b)->max_hp || !gs_of(&e)->soldiers[b].alive);
    CHECK(gs_of(&e)->combat_stats.hits > 5);
    sim_free(&e.sim);
}

static void test_no_friendly_fire_between_ai_soldiers_of_a_team(void) {
    Env e;
    CHECK(env_init(&e, false));
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 330.0f);
    const i32 b = spawn(&e, 0, SOLDIER_NORMAL, 350.0f);
    soldier_give_weapon(soldier(&e, a), WEAPON_UZI);
    soldier_give_weapon(soldier(&e, b), WEAPON_UZI);
    move_soldier(&e, 0, 300.0f);
    tick_n(&e, 300);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 0); // nobody to shoot at
    CHECK_EQ_U64(soldier(&e, a)->hp, soldier(&e, a)->max_hp);
    sim_free(&e.sim);
}

static void test_a_soldier_with_fists_runs_at_the_enemy_and_punches(void) {
    Env e;
    CHECK(env_init(&e, false));
    spawn(&e, 0, SOLDIER_NORMAL, 330.0f);
    move_soldier(&e, 0, 300.0f);
    dummy_spawn(gs_of(&e), vec2(400.0f, FLOOR_Y), TEAM_NONE);
    tick_n(&e, 600);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.kills, 1); // 8 damage a punch, 100 HP
    sim_free(&e.sim);
}

static void test_ai_does_not_throw_its_crate_at_an_enemy(void) {
    Env e;
    CHECK(env_init(&e, false));
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 330.0f);
    soldier_give_weapon(soldier(&e, a), WEAPON_PISTOL);
    move_soldier(&e, 0, 300.0f);
    const Body *b = &soldier(&e, a)->body;
    soldier(&e, a)->carrying = crate_spawn(gs_of(&e), CRATE_RED, vec2(b->pos.x + 2.0f, b->pos.y - 16.0f), CRATE_CARRIED, -1);
    gs_of(&e)->teams[0].order = ORDER_GUARD;
    dummy_spawn(gs_of(&e), vec2(400.0f, FLOOR_Y), TEAM_NONE);
    tick_n(&e, 60);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.thrown, 0);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 0); // carrying: no shooting either
    sim_free(&e.sim);
}

static void test_an_unarmed_ai_soldier_picks_up_a_weapon_nearby(void) {
    Env e;
    CHECK(env_init(&e, false));
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 330.0f);
    move_soldier(&e, 0, 300.0f);
    loot_spawn_weapon(gs_of(&e), WEAPON_UZI, 40, vec2(390.0f, FLOOR_Y - TUNING_LOOT_H));
    tick_n(&e, 200);
    CHECK(soldier(&e, a)->weapon == WEAPON_UZI && soldier(&e, a)->ammo == 40);
    CHECK_EQ_U64(count_type(&e, ENT_WEAPON), 0);
    // An armed one leaves weapons alone.
    loot_spawn_weapon(gs_of(&e), WEAPON_PISTOL, 5, vec2(430.0f, FLOOR_Y - TUNING_LOOT_H));
    tick_n(&e, 200);
    CHECK(soldier(&e, a)->weapon == WEAPON_UZI);
    CHECK_EQ_U64(count_type(&e, ENT_WEAPON), 1);
    sim_free(&e.sim);
}

// ---- AI: collecting crates -----------------------------------------------------------------------

static void test_collect_brings_crates_to_the_pad_from_the_right(void) {
    Env e;
    CHECK(env_init(&e, false));
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 330.0f);
    (void)a;
    gs_of(&e)->teams[0].order = ORDER_COLLECT;
    crate_spawn(gs_of(&e), CRATE_RED, vec2(22 * 16.0f, FLOOR_Y - 16.0f), CRATE_RESTING, -1);
    crate_spawn(gs_of(&e), CRATE_BLUE, vec2(24 * 16.0f, FLOOR_Y - 16.0f), CRATE_RESTING, -1);
    CHECK_EQ_U64(pad_crates(&e, 0), 0);
    tick_n(&e, 1500);
    CHECK_EQ_U64(pad_crates(&e, 0), 2);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.picked_up, 2);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.placed, 2);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.thrown, 0);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.destroyed, 0);
    // Coming from the right, it filled the pad from its left column.
    PadGrid g;
    pad_read_grid(gs_of(&e), 0, &g, NULL);
    CHECK(g.cells[1][0] != PAD_EMPTY && g.cells[0][0] != PAD_EMPTY);
    // ... and the pad now offers a construction.
    PadOptionList o;
    pad_world_options(gs_of(&e), 0, &o);
    CHECK(o.count > 0);
    sim_free(&e.sim);
}

static void test_collect_from_the_left_fills_the_far_column_first(void) {
    Env e;
    CHECK(env_init(&e, false));
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 80.0f);
    (void)a;
    gs_of(&e)->teams[0].order = ORDER_COLLECT;
    crate_spawn(gs_of(&e), CRATE_RED, vec2(3 * 16.0f, FLOOR_Y - 16.0f), CRATE_RESTING, -1);
    crate_spawn(gs_of(&e), CRATE_RED, vec2(5 * 16.0f, FLOOR_Y - 16.0f), CRATE_RESTING, -1);
    move_soldier(&e, 0, 800.0f);
    tick_n(&e, 1500);
    CHECK_EQ_U64(pad_crates(&e, 0), 2);
    PadGrid g;
    pad_read_grid(gs_of(&e), 0, &g, NULL);
    CHECK(g.cells[1][3] != PAD_EMPTY && g.cells[0][3] != PAD_EMPTY); // stacked in the right-hand column
    sim_free(&e.sim);
}

static void test_collect_stops_at_a_full_pad(void) {
    Env e;
    CHECK(env_init(&e, false));
    spawn(&e, 0, SOLDIER_NORMAL, 330.0f);
    move_soldier(&e, 0, 800.0f);
    gs_of(&e)->teams[0].order = ORDER_COLLECT;
    // A full pad: eight crates already there, one more lies around: it stays where it is.
    CHECK(pad_debug_fill(gs_of(&e), 0, "rrrr", "rrrr"));
    tick_n(&e, 60);
    const u32 picked = gs_of(&e)->crate_stats.picked_up;
    crate_spawn(gs_of(&e), CRATE_RED, vec2(30 * 16.0f, FLOOR_Y - 16.0f), CRATE_RESTING, -1);
    tick_n(&e, 600);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.picked_up, picked);
    CHECK_EQ_U64(pad_crates(&e, 0), 8);
    sim_free(&e.sim);
}

static void test_collect_gives_up_on_an_unreachable_crate(void) {
    Env e;
    CHECK(env_init(&e, true)); // a 4-tile wall between the soldier and the crate
    const i32 a = spawn(&e, 0, SOLDIER_NORMAL, 200.0f);
    (void)a;
    gs_of(&e)->teams[0].order = ORDER_COLLECT;
    move_soldier(&e, 0, 800.0f);
    crate_spawn(gs_of(&e), CRATE_RED, vec2(40 * 16.0f, FLOOR_Y - 16.0f), CRATE_RESTING, -1);
    // It walks toward it, bumps into the wall, gives up, and keeps cycling without ever hanging.
    tick_n(&e, 2500);
    CHECK_EQ_U64(gs_of(&e)->crate_stats.picked_up, 0);
    CHECK(gs_of(&e)->soldiers[1].alive);
    sim_free(&e.sim);
}

// ---- Determinism and invariants ------------------------------------------------------------------

static void check_squad_invariants(Env *e) {
    const GameState *gs = gs_of(e);
    const Level *lv = &e->sim.level;
    u32 alive = 0;
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        const Soldier *s = &gs->soldiers[i];
        if (!s->alive) {
            continue;
        }
        alive++;
        const Companion *c = &s->c;
        CHECK(s->team >= 0 && s->team < MAX_TEAMS && gs->teams[s->team].active);
        CHECK(c->hp > 0 && c->hp <= c->max_hp && c->max_hp == soldier_def(s->variant)->hp);
        CHECK(c->weapon >= WEAPON_FISTS && c->weapon < WEAPON_COUNT);
        CHECK(c->weapon == WEAPON_FISTS ? c->ammo == 0 : (c->ammo > 0 && c->ammo <= weapon_def(c->weapon)->max_ammo));
        CHECK(c->body.pos.x >= 0.0f && c->body.pos.x + c->body.size.x <= level_width_px(lv));
        const Vec2 ctr = body_center(&c->body);
        CHECK(!level_is_solid(lv, level_px_to_tile(ctr.x), level_px_to_tile(ctr.y)));
        if (entity_id_valid(c->carrying)) {
            const Entity *cr = pool_get_const(&gs->entities, c->carrying);
            CHECK(cr != NULL && cr->type == ENT_CRATE && cr->as.crate.state == CRATE_CARRIED);
        }
        CHECK(!gs->teams[s->team].eliminated);
    }
    CHECK_EQ_U64(alive, team_soldier_count(gs, 0) + team_soldier_count(gs, 1) + team_soldier_count(gs, 2) +
                            team_soldier_count(gs, 3));
    CHECK(gs->controlled == -1 || (gs->soldiers[gs->controlled].alive));
    if (gs->controlled >= 0) {
        CHECK(gs->soldiers[gs->controlled].team == PLAYER_TEAM);
    }
}

static u64 run_squads(u64 seed, u32 ticks, bool check) {
    Env e;
    if (!env_init(&e, false)) {
        g_failures++;
        return 0;
    }
    e.sim.use_bot = true;
    bot_init(&e.sim.bot, seed);
    for (i32 t = 0; t < 2; t++) {
        spawn(&e, 0, (SoldierVariant)(1 + t * 2 % 3), 120.0f + (f32)t * 40.0f);
        spawn(&e, 1, (SoldierVariant)t, 760.0f - (f32)t * 40.0f);
    }
    for (u32 t = 0; t < ticks; t++) {
        if (t % 400 == 0) {
            gs_of(&e)->teams[0].order = (Order)((t / 400) % ORDER_COUNT);
            gs_of(&e)->teams[1].order = (Order)((t / 400 + 1) % ORDER_COUNT);
            const Vec2 where = vec2(300.0f + (f32)(t % 7) * 40.0f, FLOOR_Y - 16.0f);
            crate_spawn(gs_of(&e), CRATE_RED, where, CRATE_FALLING, -1);
        }
        if (t % 700 == 0) { // keep the pads' owners armed so the fights go on
            for (i32 i = 0; i < MAX_SOLDIERS; i++) {
                if (gs_of(&e)->soldiers[i].alive && gs_of(&e)->soldiers[i].c.weapon == WEAPON_FISTS) {
                    soldier_give_weapon(&gs_of(&e)->soldiers[i].c, (WeaponKind)(1 + (t / 700 + (u32)i) % 5));
                }
            }
        }
        sim_tick(&e.sim, NULL);
        for (i32 t2 = 0; t2 < 2; t2++) { // a team that lost its last soldier gets a new one (as its pad would make)
            if (gs_of(&e)->teams[t2].eliminated) {
                CHECK(soldier_spawn(gs_of(&e), t2, (SoldierVariant)(t % SOLDIER_VARIANT_COUNT),
                                    gs_of(&e)->teams[t2].spawn) >= 0);
            }
        }
        if (check) {
            check_squad_invariants(&e);
        }
    }
    const u64 hash = game_hash(gs_of(&e));
    sim_free(&e.sim);
    return hash;
}

static void test_squads_under_a_bot_keep_their_invariants_and_are_deterministic(void) {
    const u64 a = run_squads(3, 5000, true);
    const u64 b = run_squads(3, 5000, false);
    CHECK(a == b);
    CHECK(run_squads(4, 5000, false) != a);
}

static void test_the_hash_sees_the_ai_soldiers(void) {
    Env e1, e2;
    CHECK(env_init(&e1, false) && env_init(&e2, false));
    spawn(&e1, 0, SOLDIER_NORMAL, 300.0f);
    spawn(&e2, 0, SOLDIER_NORMAL, 300.0f);
    tick_n(&e1, 30);
    tick_n(&e2, 30);
    CHECK(game_hash(gs_of(&e1)) == game_hash(gs_of(&e2)));
    gs_of(&e2)->soldiers[1].c.hp = 99;
    CHECK(game_hash(gs_of(&e1)) != game_hash(gs_of(&e2)));
    gs_of(&e2)->soldiers[1].c.hp = gs_of(&e1)->soldiers[1].c.hp;
    CHECK(game_hash(gs_of(&e1)) == game_hash(gs_of(&e2)));
    gs_of(&e2)->soldiers[1].variant = SOLDIER_HEAVY;
    CHECK(game_hash(gs_of(&e1)) != game_hash(gs_of(&e2)));
    gs_of(&e2)->soldiers[1].variant = SOLDIER_NORMAL;
    gs_of(&e2)->teams[0].order = ORDER_ATTACK;
    CHECK(game_hash(gs_of(&e1)) != game_hash(gs_of(&e2)));
    sim_free(&e1.sim);
    sim_free(&e2.sim);
}

static void test_restart_starts_the_match_over(void) {
    Env e;
    CHECK(env_init(&e, false));
    spawn(&e, 0, SOLDIER_HEAVY, 300.0f);
    CHECK(pad_debug_fill(gs_of(&e), 0, "rr..", "rr.."));
    tick_n(&e, 100);
    soldier(&e, 0)->hp = 0;
    soldier(&e, 1)->hp = 0;
    tick(&e, 0);
    CHECK(gs_of(&e)->teams[0].eliminated);
    sim_restart(&e.sim);
    CHECK_EQ_U64(team_soldier_count(gs_of(&e), 0), 1);
    CHECK(gs_of(&e)->controlled == 0 && !gs_of(&e)->teams[0].eliminated);
    CHECK_EQ_U64(gs_of(&e)->tick, 0);
    CHECK_EQ_U64(crate_count(gs_of(&e)), 0);
    CHECK_EQ_U64(pad_crates(&e, 0), 0); // the resting crates of the old match left no trace in the grid
    CHECK_EQ_U64(gs_of(&e)->combat_stats.deaths, 0);
    tick_n(&e, 30);
    sim_free(&e.sim);
}

int main(void) {
    RUN_TEST(test_variants_hit_points_and_equipment);
    RUN_TEST(test_products_map_to_variants);
    RUN_TEST(test_the_pad_makes_every_variant);
    RUN_TEST(test_a_pad_without_room_builds_nothing);
    RUN_TEST(test_spawn_checks_the_room_and_the_arguments);
    RUN_TEST(test_the_game_starts_with_one_soldier_for_team_one);
    RUN_TEST(test_switch_cycles_through_the_team);
    RUN_TEST(test_switch_with_one_soldier_changes_nothing);
    RUN_TEST(test_tab_belongs_to_the_pad_menu_while_it_is_up);
    RUN_TEST(test_the_controlled_soldier_is_the_one_that_obeys_the_keys);
    RUN_TEST(test_death_passes_control_to_the_closest_ally);
    RUN_TEST(test_a_team_is_eliminated_with_its_last_soldier);
    RUN_TEST(test_water_sends_any_soldier_back_to_its_spawn);
    RUN_TEST(test_the_order_key_cycles_the_team_order);
    RUN_TEST(test_follow_keeps_close_to_the_controlled_soldier);
    RUN_TEST(test_guard_stays_near_the_base);
    RUN_TEST(test_attack_goes_to_the_enemy_base_and_hits_it);
    RUN_TEST(test_ai_soldiers_shoot_enemies_in_range);
    RUN_TEST(test_ai_does_not_shoot_through_walls);
    RUN_TEST(test_ai_does_not_shoot_out_of_range);
    RUN_TEST(test_soldiers_of_two_teams_fight_each_other);
    RUN_TEST(test_no_friendly_fire_between_ai_soldiers_of_a_team);
    RUN_TEST(test_a_soldier_with_fists_runs_at_the_enemy_and_punches);
    RUN_TEST(test_ai_does_not_throw_its_crate_at_an_enemy);
    RUN_TEST(test_an_unarmed_ai_soldier_picks_up_a_weapon_nearby);
    RUN_TEST(test_collect_brings_crates_to_the_pad_from_the_right);
    RUN_TEST(test_collect_from_the_left_fills_the_far_column_first);
    RUN_TEST(test_collect_stops_at_a_full_pad);
    RUN_TEST(test_collect_gives_up_on_an_unreachable_crate);
    RUN_TEST(test_squads_under_a_bot_keep_their_invariants_and_are_deterministic);
    RUN_TEST(test_the_hash_sees_the_ai_soldiers);
    RUN_TEST(test_restart_starts_the_match_over);
    TEST_MAIN_END();
}
