// Bases (hit points, what damages them, a destroyed base locks its pad), team statistics and the end of a match.
#include "test.h"

#include "app/sim.h"
#include "game/base.h"
#include "game/combat.h"
#include "game/crate.h"
#include "game/pad.h"
#include "game/rules.h"
#include "game/squad.h"
#include "game/tuning.h"
#include "test_helpers.h"

typedef struct Env {
    Sim sim;
    InputFrame in;
} Env;

#define FLOOR_Y 192.0f // feet on the floor (rows 12-13 are solid)

// 60x14, flat. Team 1 spawns at column 1 (pad at 8-11), team 2 at column 57 (pad at 48-51).
// `third`: a team-3 spawn at column 30, without a pad.
static bool env_init(Env *e, bool third, bool dummy) {
    TestMap m;
    map_init(&m, 60, 14);
    map_rect(&m, 0, 12, 59, 13, '#');
    map_put(&m, 1, 11, '1');
    map_put(&m, 57, 11, '2');
    map_put(&m, 8, 11, 'P');
    map_put(&m, 48, 11, 'P');
    if (third) {
        map_put(&m, 30, 11, '3');
    }
    if (dummy) {
        map_put(&m, 20, 11, 'T');
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

static Vec2 base_center(Env *e, i32 team) {
    const Rect r = base_rect(gs_of(e), team);
    return vec2(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
}

// ---- Bases -------------------------------------------------------------------------------------

static void test_teams_with_a_spawn_and_a_pad_have_a_base(void) {
    Env e;
    CHECK(env_init(&e, true, false));
    const GameState *gs = gs_of(&e);
    CHECK(gs->teams[0].has_base && gs->teams[0].base_hp == TUNING_BASE_HP);
    CHECK(gs->teams[1].has_base && gs->teams[1].base_hp == TUNING_BASE_HP);
    CHECK(!gs->teams[2].has_base && !base_alive(gs, 2)); // a spawn without a pad
    CHECK(!gs->teams[3].has_base);
    const Rect r = base_rect(gs, 1); // stands on the spawn
    CHECK_NEAR(r.x + r.w * 0.5f, gs->teams[1].spawn.x, 0.01f);
    CHECK_NEAR(r.y + r.h, gs->teams[1].spawn.y, 0.01f);
    CHECK_NEAR(r.w, TUNING_BASE_W, 0.01f);
    CHECK(base_rect(gs, 2).w == 0.0f);
    sim_free(&e.sim);
}

static void test_enemy_bullets_hit_a_base_and_own_bullets_do_not(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    GameState *gs = gs_of(&e);
    const Vec2 c = base_center(&e, 1);
    projectile_spawn(gs, WEAPON_PISTOL, 1, vec2(c.x - 40.0f, c.y), vec2(300.0f, 0.0f)); // its own team
    tick_n(&e, 30);
    CHECK(gs->teams[1].base_hp == TUNING_BASE_HP);
    projectile_spawn(gs, WEAPON_PISTOL, 0, vec2(c.x - 40.0f, c.y), vec2(300.0f, 0.0f));
    tick_n(&e, 30);
    CHECK(gs->teams[1].base_hp == TUNING_BASE_HP - weapon_def(WEAPON_PISTOL)->damage);
    CHECK(gs->teams[0].stats.base_damage == (u32)weapon_def(WEAPON_PISTOL)->damage);
    CHECK(projectile_count(gs) == 0); // the bullet stopped on the building
    sim_free(&e.sim);
}

static void test_explosions_spare_their_own_base_only(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    GameState *gs = gs_of(&e);
    const Vec2 c = base_center(&e, 1);
    combat_explode_by(gs, c, WEAPON_GRENADES, 1);
    CHECK(gs->teams[1].base_hp == TUNING_BASE_HP);
    combat_explode_by(gs, c, WEAPON_GRENADES, 0);
    const i32 after_enemy = gs->teams[1].base_hp;
    CHECK(after_enemy == TUNING_BASE_HP - weapon_def(WEAPON_GRENADES)->damage); // dead center: full damage
    combat_explode(gs, c, WEAPON_GRENADES); // nobody's: hurts it too
    CHECK(gs->teams[1].base_hp < after_enemy);
    CHECK(gs->teams[1].base_hurt_ticks > 0);
    sim_free(&e.sim);
}

static void test_a_destroyed_base_locks_its_pad(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    GameState *gs = gs_of(&e);
    CHECK(pad_debug_fill(gs, 1, "....", "rr.."));
    tick_n(&e, 2);
    PadOptionList options;
    pad_world_options(gs, 1, &options);
    CHECK(options.count > 0 && pad_usable(gs, 1));

    events_clear(&gs->events);
    CHECK(base_damage(gs, 1, 5000, 0) == TUNING_BASE_HP); // no more than it had
    CHECK(gs->teams[1].base_hp == 0 && !base_alive(gs, 1));
    CHECK(events_count_of(&gs->events, EV_BASE_DESTROYED) == 1);
    CHECK(base_damage(gs, 1, 10, 0) == 0); // a ruin takes no more damage
    CHECK(events_count_of(&gs->events, EV_BASE_DESTROYED) == 1);
    CHECK(!pad_usable(gs, 1));
    pad_world_options(gs, 1, &options);
    CHECK(options.count == 0);
    CHECK(!pad_build(gs, 1, SHAPE_2X1_H, false));
    CHECK(pad_crate_count(gs, 1) == 2); // the crates stay: anybody can still take them
    CHECK(pad_usable(gs, 0)); // the other team is not concerned

    // Bullets fly through the ruin.
    const Vec2 c = base_center(&e, 1);
    const u32 hits = gs->combat_stats.hits;
    projectile_spawn(gs, WEAPON_PISTOL, 0, vec2(c.x - 30.0f, c.y), vec2(300.0f, 0.0f));
    tick_n(&e, 10);
    CHECK(gs->combat_stats.hits == hits);
    sim_free(&e.sim);
}

static void test_a_base_without_defenders_falls_to_an_attacker(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    GameState *gs = gs_of(&e);
    const i32 a = soldier_spawn(gs, 0, SOLDIER_NORMAL, vec2(300.0f, FLOOR_Y));
    CHECK(a >= 0);
    gs->teams[0].order = ORDER_ATTACK;
    for (int i = 0; i < 6000 && base_alive(gs, 1); i++) {
        tick_n(&e, 1);
    }
    CHECK(!base_alive(gs, 1)); // punched down (8 per punch, 2.5 punches a second)
    CHECK(gs->teams[0].stats.base_damage == TUNING_BASE_HP);
    CHECK(!gs->teams[1].eliminated); // a team is not out because its base is: it is out when its soldiers are
    CHECK(!gs->match.over);
    sim_free(&e.sim);
}

static void test_a_base_raises_one_alert_per_attack(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    GameState *gs = gs_of(&e);
    events_clear(&gs->events);
    base_damage(gs, 1, 10, 0);
    base_damage(gs, 1, 10, 0);
    CHECK(events_count_of(&gs->events, EV_BASE_ATTACKED) == 1);
    CHECK(gs->events.items[0].type == EV_BASE_ATTACKED && gs->events.items[0].a == 1 && gs->events.items[0].b == 0);
    tick_n(&e, (u32)SECONDS_TO_TICKS(TUNING_BASE_ALERT_S) + 2); // the alert is over
    events_clear(&gs->events);
    base_damage(gs, 1, 10, 0);
    CHECK(events_count_of(&gs->events, EV_BASE_ATTACKED) == 1);
    sim_free(&e.sim);
}

static void test_the_hash_sees_the_base(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    GameState *gs = gs_of(&e);
    const u64 before = game_hash(gs);
    base_damage(gs, 1, 1, 0);
    CHECK(game_hash(gs) != before);
    sim_free(&e.sim);
}

// ---- Team statistics ---------------------------------------------------------------------------

static void test_kills_losses_and_constructions_are_counted_per_team(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    GameState *gs = gs_of(&e);
    const Companion *me = game_player_const(gs);
    const f32 x = me->body.pos.x + me->body.size.x + 6.0f + TUNING_COMPANION_W * 0.5f;
    const i32 foe = soldier_spawn(gs, 1, SOLDIER_NORMAL, vec2(x, FLOOR_Y));
    CHECK(foe >= 0);
    gs->soldiers[foe].c.hp = 5;
    gs->teams[0].order = ORDER_GUARD;
    for (int i = 0; i < 4; i++) { // punch it (the player faces right at the start)
        input_frame_update(&e.in, i == 0 ? KEY(ACT_FIRE) : 0);
        sim_tick(&e.sim, &e.in);
    }
    CHECK(!gs->soldiers[foe].alive);
    CHECK(gs->teams[0].stats.kills == 1 && gs->teams[1].stats.losses == 1);
    CHECK(gs->teams[0].stats.losses == 0 && gs->teams[1].stats.kills == 0);

    CHECK(pad_debug_fill(gs, 0, "r...", "r..."));
    tick_n(&e, 2);
    CHECK(pad_build(gs, 0, SHAPE_2X1_V, false));
    CHECK(gs->teams[0].stats.built == 1 && gs->teams[1].stats.built == 0);
    sim_free(&e.sim);
}

static void test_a_soldier_killed_by_its_own_grenade_credits_nobody(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    GameState *gs = gs_of(&e);
    const i32 mate = soldier_spawn(gs, 0, SOLDIER_NORMAL, vec2(200.0f, FLOOR_Y));
    gs->soldiers[mate].c.hp = 1;
    combat_explode_by(gs, body_center(&gs->soldiers[mate].c.body), WEAPON_GRENADES, 0);
    tick_n(&e, 1);
    CHECK(!gs->soldiers[mate].alive);
    CHECK(gs->teams[0].stats.kills == 0 && gs->teams[0].stats.losses == 1);
    sim_free(&e.sim);
}

// ---- End of the match --------------------------------------------------------------------------

static void test_a_match_played_alone_never_ends(void) {
    Env e;
    CHECK(env_init(&e, false, true));
    GameState *gs = gs_of(&e);
    CHECK(rules_teams_playing(gs) == 1);
    tick_n(&e, 300);
    CHECK(!gs->match.over);
    sim_free(&e.sim);
}

static void test_the_last_team_standing_wins(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    GameState *gs = gs_of(&e);
    const i32 foe = soldier_spawn(gs, 1, SOLDIER_NORMAL, vec2(800.0f, FLOOR_Y));
    tick_n(&e, 1);
    CHECK(rules_teams_playing(gs) == 2 && rules_teams_left(gs) == 2 && !gs->match.over);
    gs->soldiers[foe].c.hp = 0;
    input_frame_update(&e.in, 0);
    sim_tick(&e.sim, &e.in);
    CHECK(gs->match.over && gs->match.winner == 0);
    CHECK(gs->match.end_tick == gs->tick - 1);
    CHECK(events_count_of(&gs->events, EV_MATCH_OVER) == 1);
    const u64 end = gs->match.end_tick;
    tick_n(&e, 1);
    CHECK(events_count_of(&gs->events, EV_MATCH_OVER) == 0); // once
    // Latched, even if the loser gets a soldier back.
    soldier_spawn(gs, 1, SOLDIER_NORMAL, vec2(800.0f, FLOOR_Y));
    tick_n(&e, 5);
    CHECK(gs->match.over && gs->match.winner == 0 && gs->match.end_tick == end);
    sim_free(&e.sim);
}

static void test_the_human_loses_with_their_last_soldier(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    GameState *gs = gs_of(&e);
    soldier_spawn(gs, 1, SOLDIER_NORMAL, vec2(800.0f, FLOOR_Y));
    game_player(gs)->hp = 0;
    tick_n(&e, 1);
    CHECK(gs->controlled < 0);
    CHECK(gs->match.over && gs->match.winner == 1);
    sim_free(&e.sim);
}

static void test_teams_falling_together_is_a_draw(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    GameState *gs = gs_of(&e);
    const i32 foe = soldier_spawn(gs, 1, SOLDIER_NORMAL, vec2(800.0f, FLOOR_Y));
    gs->soldiers[foe].c.hp = 0;
    game_player(gs)->hp = 0;
    tick_n(&e, 1);
    CHECK(gs->match.over && gs->match.winner == -1);
    sim_free(&e.sim);
}

static void test_three_teams_play_until_one_is_left(void) {
    Env e;
    CHECK(env_init(&e, true, false));
    GameState *gs = gs_of(&e);
    const i32 b = soldier_spawn(gs, 1, SOLDIER_NORMAL, vec2(800.0f, FLOOR_Y));
    const i32 c = soldier_spawn(gs, 2, SOLDIER_NORMAL, vec2(480.0f, FLOOR_Y));
    tick_n(&e, 1);
    CHECK(rules_teams_playing(gs) == 3);
    gs->soldiers[b].c.hp = 0;
    tick_n(&e, 1);
    CHECK(!gs->match.over && rules_teams_left(gs) == 2);
    gs->soldiers[c].c.hp = 0;
    tick_n(&e, 1);
    CHECK(gs->match.over && gs->match.winner == 0);
    sim_free(&e.sim);
}

// ---- The commander and its base ----------------------------------------------------------------

static void test_a_team_whose_base_is_under_fire_stays_home(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    GameState *gs = gs_of(&e);
    game_set_team_ai(gs, 1, true);
    for (int i = 0; i < 4; i++) {
        soldier_spawn(gs, 1, SOLDIER_HEAVY, vec2(700.0f + 20.0f * (f32)i, FLOOR_Y));
    }
    tick_n(&e, 31);
    bool attacking = false;
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        attacking |= gs->soldiers[i].alive && gs->soldiers[i].team == 1 && gs->soldiers[i].ai.role == ORDER_ATTACK;
    }
    CHECK(attacking); // heavies attack...
    base_damage(gs, 1, 1, 0);
    tick_n(&e, 31);
    attacking = false;
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        attacking |= gs->soldiers[i].alive && gs->soldiers[i].team == 1 && gs->soldiers[i].ai.role == ORDER_ATTACK;
    }
    CHECK(!attacking); // ... unless their base is being shot at
    sim_free(&e.sim);
}

static void test_a_team_without_a_base_does_not_collect(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    GameState *gs = gs_of(&e);
    game_set_team_ai(gs, 1, true);
    base_damage(gs, 1, TUNING_BASE_HP, 0);
    tick_n(&e, SECONDS_TO_TICKS(TUNING_BASE_ALERT_S) + 2);
    crate_spawn(gs, CRATE_RED, vec2(600.0f, FLOOR_Y - 16.0f), CRATE_FALLING, -1);
    tick_n(&e, 61);
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        if (gs->soldiers[i].alive && gs->soldiers[i].team == 1) {
            CHECK(gs->soldiers[i].ai.role != ORDER_COLLECT && gs->soldiers[i].ai.role != ORDER_STEAL);
        }
    }
    sim_free(&e.sim);
}

int main(void) {
    RUN_TEST(test_teams_with_a_spawn_and_a_pad_have_a_base);
    RUN_TEST(test_enemy_bullets_hit_a_base_and_own_bullets_do_not);
    RUN_TEST(test_explosions_spare_their_own_base_only);
    RUN_TEST(test_a_destroyed_base_locks_its_pad);
    RUN_TEST(test_a_base_without_defenders_falls_to_an_attacker);
    RUN_TEST(test_a_base_raises_one_alert_per_attack);
    RUN_TEST(test_the_hash_sees_the_base);
    RUN_TEST(test_kills_losses_and_constructions_are_counted_per_team);
    RUN_TEST(test_a_soldier_killed_by_its_own_grenade_credits_nobody);
    RUN_TEST(test_a_match_played_alone_never_ends);
    RUN_TEST(test_the_last_team_standing_wins);
    RUN_TEST(test_the_human_loses_with_their_last_soldier);
    RUN_TEST(test_teams_falling_together_is_a_draw);
    RUN_TEST(test_three_teams_play_until_one_is_left);
    RUN_TEST(test_a_team_whose_base_is_under_fire_stays_home);
    RUN_TEST(test_a_team_without_a_base_does_not_collect);
    TEST_MAIN_END();
}
