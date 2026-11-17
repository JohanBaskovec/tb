// Combat: the weapon table, shooting, projectiles, explosions, damage, dummies, pickup, death,
// and a bot run that checks the invariants over thousands of ticks.
#include "test.h"

#include "app/sim.h"
#include "game/combat.h"
#include "game/crate.h"
#include "game/loot.h"
#include "game/tuning.h"
#include "game/pad.h"
#include "test_helpers.h"

// ---- Environment ---------------------------------------------------------------------------------

typedef struct Env {
    Sim sim;
    InputFrame in;
} Env;

#define FLOOR_Y 160.0f // top of the floor (rows 10-11 of the 40x12 map)

// 40x12 map with a floor. Spawn at column 1. Optional wall at column 5 (x 80..96, rows 6-9) and pad at column 20.
static bool env_init(Env *e, bool wall, bool pad) {
    TestMap m;
    map_init(&m, 40, 12);
    map_rect(&m, 0, 10, 39, 11, '#');
    map_put(&m, 1, 9, '1');
    if (wall) {
        map_rect(&m, 5, 6, 5, 9, '#');
    }
    if (pad) {
        map_put(&m, 20, 9, 'P');
    }
    char text[8192];
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

static Companion *player(Env *e) {
    return &e->sim.gs.player;
}

static void place_player(Env *e, f32 feet_x) {
    companion_init(player(e), vec2(feet_x, FLOOR_Y));
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

// Ticks until the first event of `type` appears (at most `max` ticks); returns the tick count or -1.
static int ticks_until_event(Env *e, EventType type, int max) {
    for (int i = 1; i <= max; i++) {
        tick(e, 0);
        if (events_count_of(&gs_of(e)->events, type) > 0) {
            return i;
        }
    }
    return -1;
}

static const Entity *dummy_at(Env *e, int nth) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *d = &gs_of(e)->entities.slots[i];
        if (d->type == ENT_DUMMY && nth-- == 0) {
            return d;
        }
    }
    return NULL;
}

static u32 count_type(Env *e, EntityType type) {
    u32 n = 0;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        n += gs_of(e)->entities.slots[i].type == type ? 1u : 0u;
    }
    return n;
}

// ---- The weapon table ----------------------------------------------------------------------------

static void test_weapon_table_is_sane(void) {
    for (int k = 0; k < WEAPON_COUNT; k++) {
        const WeaponDef *d = weapon_def((WeaponKind)k);
        CHECK(d->name != NULL && d->name[0] != '\0');
        CHECK(d->cooldown_ticks > 0);
        CHECK(d->damage > 0);
        if (k == WEAPON_FISTS) {
            CHECK(d->shot == SHOT_MELEE && d->max_ammo == 0 && d->reach > 0.0f);
            continue;
        }
        CHECK(d->max_ammo > 0); // every weapon but the fists has limited ammunition
        CHECK(d->shot != SHOT_MELEE);
        CHECK(d->speed > 0.0f && d->life_ticks > 0 && d->proj_w > 0.0f && d->proj_h > 0.0f);
        CHECK((d->shot == SHOT_BULLET) == (d->radius == 0.0f)); // bullets do not explode, the others do
        CHECK((d->shot == SHOT_GRENADE) == (d->lift > 0.0f));
    }
    CHECK(weapon_def(WEAPON_STRONG_GRENADES)->radius > weapon_def(WEAPON_GRENADES)->radius);
    CHECK(weapon_def(WEAPON_STRONG_GRENADES)->damage > weapon_def(WEAPON_GRENADES)->damage);
    CHECK(weapon_def(WEAPON_UZI)->automatic && !weapon_def(WEAPON_PISTOL)->automatic);
    CHECK(weapon_def(WEAPON_UZI)->max_ammo > weapon_def(WEAPON_PISTOL)->max_ammo);
    CHECK(weapon_def((WeaponKind)99) == weapon_def(WEAPON_FISTS)); // out of range: the fallback
}

static void test_pad_products_map_to_weapons(void) {
    WeaponKind w;
    CHECK(weapon_from_product(PRODUCT_PISTOL, &w) && w == WEAPON_PISTOL);
    CHECK(weapon_from_product(PRODUCT_UZI, &w) && w == WEAPON_UZI);
    CHECK(weapon_from_product(PRODUCT_GRENADES, &w) && w == WEAPON_GRENADES);
    CHECK(weapon_from_product(PRODUCT_STRONG_GRENADES, &w) && w == WEAPON_STRONG_GRENADES);
    CHECK(weapon_from_product(PRODUCT_ROCKET_LAUNCHER, &w) && w == WEAPON_ROCKET_LAUNCHER);
    CHECK(!weapon_from_product(PRODUCT_COMPANION, &w));
    CHECK(!weapon_from_product(PRODUCT_TANK, &w));
}

// ---- Bullets -------------------------------------------------------------------------------------

static void test_pistol_shoots_one_bullet_per_press(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 40.0f);
    combat_give_weapon(gs_of(&e), WEAPON_PISTOL);
    CHECK_EQ_U64(player(&e)->ammo, 24);

    tick(&e, KEY(ACT_FIRE));
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 1);
    CHECK_EQ_U64(player(&e)->ammo, 23);
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_SHOT), 1);
    CHECK_EQ_U64(projectile_count(gs_of(&e)), 1);

    // Holding the key does nothing more (semi-automatic); releasing and pressing within the
    // cooldown does nothing either; after the cooldown a new press fires again.
    for (int i = 0; i < 30; i++) {
        tick(&e, KEY(ACT_FIRE));
    }
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 1);
    tick(&e, 0);
    tick(&e, KEY(ACT_FIRE));
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 2);
    sim_free(&e.sim);
}

static void test_bullet_hits_a_dummy_and_pushes_nothing_else(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 40.0f);
    dummy_spawn(gs_of(&e), vec2(110.0f, FLOOR_Y), 1);
    combat_give_weapon(gs_of(&e), WEAPON_PISTOL);
    tick(&e, KEY(ACT_FIRE));
    const int t = ticks_until_event(&e, EV_PROJECTILE_HIT, 40);
    CHECK(t > 5 && t < 25); // about 55 px at 360 px/s
    CHECK_EQ_U64(dummy_at(&e, 0)->as.dummy.hp, TUNING_DUMMY_HP - weapon_def(WEAPON_PISTOL)->damage);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.hits, 1);
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_DAMAGE), 1);
    CHECK_EQ_U64(player(&e)->hp, TUNING_SOLDIER_HP); // no friendly fire from bullets
    tick_n(&e, 2);
    CHECK_EQ_U64(projectile_count(gs_of(&e)), 0); // the bullet is gone
    sim_free(&e.sim);
}

static void test_bullet_stops_at_a_wall(void) {
    Env e;
    CHECK(env_init(&e, true, false)); // wall at x 80..96
    place_player(&e, 40.0f);
    dummy_spawn(gs_of(&e), vec2(140.0f, FLOOR_Y), 1); // behind the wall
    combat_give_weapon(gs_of(&e), WEAPON_PISTOL);
    tick(&e, KEY(ACT_FIRE));
    CHECK(ticks_until_event(&e, EV_PROJECTILE_HIT, 40) > 0);
    tick_n(&e, 40);
    CHECK_EQ_U64(dummy_at(&e, 0)->as.dummy.hp, TUNING_DUMMY_HP);
    CHECK_EQ_U64(projectile_count(gs_of(&e)), 0);
    sim_free(&e.sim);
}

static void test_bullet_range_ends(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 40.0f);
    combat_give_weapon(gs_of(&e), WEAPON_PISTOL);
    tick(&e, KEY(ACT_FIRE));
    CHECK_EQ_U64(projectile_count(gs_of(&e)), 1);
    tick_n(&e, (int)weapon_def(WEAPON_PISTOL)->life_ticks + 2);
    CHECK_EQ_U64(projectile_count(gs_of(&e)), 0);
    sim_free(&e.sim);
}

static void test_shooting_left(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 200.0f);
    dummy_spawn(gs_of(&e), vec2(140.0f, FLOOR_Y), 1);
    combat_give_weapon(gs_of(&e), WEAPON_PISTOL);
    tick(&e, KEY(ACT_LEFT)); // face left
    tick(&e, 0);
    CHECK(player(&e)->facing == -1);
    tick(&e, KEY(ACT_FIRE));
    CHECK(ticks_until_event(&e, EV_PROJECTILE_HIT, 40) > 0);
    CHECK(dummy_at(&e, 0)->as.dummy.hp < TUNING_DUMMY_HP);
    sim_free(&e.sim);
}

static void test_uzi_is_automatic_and_spreads(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 40.0f);
    combat_give_weapon(gs_of(&e), WEAPON_UZI);
    f32 min_vy = 1e9f, max_vy = -1e9f;
    for (int i = 0; i < 60; i++) {
        tick(&e, KEY(ACT_FIRE));
        for (u16 s = 1; s <= MAX_ENTITIES; s++) {
            const Entity *p = &gs_of(&e)->entities.slots[s];
            if (p->type == ENT_PROJECTILE) {
                min_vy = MIN(min_vy, p->body.vel.y);
                max_vy = MAX(max_vy, p->body.vel.y);
            }
        }
    }
    // Ticks 0, 5, 10, ... : 12 shots in 60 ticks.
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 12);
    CHECK_EQ_U64(player(&e)->ammo, 60 - 12);
    CHECK(min_vy < -1.0f && max_vy > 1.0f); // the burst fans out
    const f32 limit = weapon_def(WEAPON_UZI)->spread * weapon_def(WEAPON_UZI)->speed;
    CHECK(min_vy >= -limit && max_vy <= limit);
    sim_free(&e.sim);
}

static void test_running_out_of_ammo_goes_back_to_fists(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 40.0f);
    combat_give_weapon(gs_of(&e), WEAPON_PISTOL);
    for (int shot = 0; shot < 24; shot++) {
        tick(&e, KEY(ACT_FIRE));
        tick_n(&e, 20); // past the cooldown, releases the key
    }
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 24);
    CHECK(player(&e)->weapon == WEAPON_FISTS);
    // The weapon is gone for good: the next press is a punch, not a shot.
    tick(&e, KEY(ACT_FIRE));
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 24);
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_PUNCH), 1);
    sim_free(&e.sim);
}

static void test_empty_event_is_sent_with_the_last_shot(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 40.0f);
    combat_give_weapon(gs_of(&e), WEAPON_PISTOL);
    player(&e)->ammo = 1;
    tick(&e, KEY(ACT_FIRE));
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_WEAPON_EMPTY), 1);
    CHECK(player(&e)->weapon == WEAPON_FISTS);
    sim_free(&e.sim);
}

// ---- Fists ---------------------------------------------------------------------------------------

static void test_fists_hit_within_reach_only(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 40.0f); // body spans x 34..46, reach 14: up to x 60
    dummy_spawn(gs_of(&e), vec2(53.0f, FLOOR_Y), 1);  // x 46..60: in reach
    dummy_spawn(gs_of(&e), vec2(100.0f, FLOOR_Y), 1); // far
    tick(&e, KEY(ACT_FIRE));
    CHECK_EQ_U64(dummy_at(&e, 0)->as.dummy.hp, TUNING_DUMMY_HP - weapon_def(WEAPON_FISTS)->damage);
    CHECK_EQ_U64(dummy_at(&e, 1)->as.dummy.hp, TUNING_DUMMY_HP);
    CHECK(player(&e)->punch_ticks > 0);
    // Cooldown: a second press right away does nothing.
    tick(&e, 0);
    tick(&e, KEY(ACT_FIRE));
    CHECK_EQ_U64(dummy_at(&e, 0)->as.dummy.hp, TUNING_DUMMY_HP - weapon_def(WEAPON_FISTS)->damage);
    tick_n(&e, (int)weapon_def(WEAPON_FISTS)->cooldown_ticks);
    tick(&e, KEY(ACT_FIRE));
    CHECK_EQ_U64(dummy_at(&e, 0)->as.dummy.hp, TUNING_DUMMY_HP - 2 * weapon_def(WEAPON_FISTS)->damage);
    sim_free(&e.sim);
}

static void test_fists_infinite(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 40.0f);
    for (int i = 0; i < 10; i++) {
        tick(&e, KEY(ACT_FIRE));
        tick_n(&e, 30);
    }
    CHECK(player(&e)->weapon == WEAPON_FISTS);
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_WEAPON_EMPTY), 0);
    sim_free(&e.sim);
}

// ---- Explosions ----------------------------------------------------------------------------------

static void test_explosion_damage_falls_off_with_distance(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 8.0f);
    const WeaponDef *g = weapon_def(WEAPON_GRENADES); // radius 40, damage 60, edge at 40 %
    dummy_spawn(gs_of(&e), vec2(200.0f, FLOOR_Y), 1);
    dummy_spawn(gs_of(&e), vec2(300.0f, FLOOR_Y), 1);
    dummy_spawn(gs_of(&e), vec2(380.0f, FLOOR_Y), 1);
    const Vec2 c0 = body_center(&dummy_at(&e, 0)->body);
    combat_explode(gs_of(&e), c0, WEAPON_GRENADES); // dead center
    CHECK_EQ_U64(dummy_at(&e, 0)->as.dummy.hp, TUNING_DUMMY_HP - g->damage);

    // 20 px from the nearest edge of the second one: 1 - 0.6 * 20/40 = 70 % of the damage.
    const Entity *d1 = dummy_at(&e, 1);
    combat_explode(gs_of(&e), vec2(d1->body.pos.x - 20.0f, d1->body.pos.y + 12.0f), WEAPON_GRENADES);
    CHECK_EQ_U64(dummy_at(&e, 1)->as.dummy.hp, TUNING_DUMMY_HP - (i32)((f32)g->damage * 0.7f + 0.5f));

    // Out of the radius: untouched.
    const Entity *d2 = dummy_at(&e, 2);
    combat_explode(gs_of(&e), vec2(d2->body.pos.x - 41.0f, d2->body.pos.y + 12.0f), WEAPON_GRENADES);
    CHECK_EQ_U64(dummy_at(&e, 2)->as.dummy.hp, TUNING_DUMMY_HP);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.explosions, 3);
    sim_free(&e.sim);
}

static void test_stronger_explosions_hurt_more_and_reach_further(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 8.0f);
    dummy_spawn(gs_of(&e), vec2(200.0f, FLOOR_Y), 1);
    dummy_spawn(gs_of(&e), vec2(400.0f, FLOOR_Y), 1);
    const Entity *a = dummy_at(&e, 0), *b = dummy_at(&e, 1);
    const Vec2 pa = vec2(a->body.pos.x - 50.0f, a->body.pos.y + 12.0f);
    const Vec2 pb = vec2(b->body.pos.x - 50.0f, b->body.pos.y + 12.0f);
    combat_explode(gs_of(&e), pa, WEAPON_GRENADES);        // 50 px: out of reach
    combat_explode(gs_of(&e), pb, WEAPON_STRONG_GRENADES); // 50 px: within 64
    CHECK_EQ_U64(dummy_at(&e, 0)->as.dummy.hp, TUNING_DUMMY_HP);
    CHECK(dummy_at(&e, 1)->as.dummy.hp < TUNING_DUMMY_HP);
    sim_free(&e.sim);
}

static void test_walls_block_explosions(void) {
    Env e;
    CHECK(env_init(&e, true, false)); // wall at x 80..96
    place_player(&e, 8.0f);
    dummy_spawn(gs_of(&e), vec2(106.0f, FLOOR_Y), 1); // right behind the wall
    const Entity *d = dummy_at(&e, 0);
    // 10 px from the dummy's edge but with the wall in between.
    combat_explode(gs_of(&e), vec2(76.0f, d->body.pos.y + 12.0f), WEAPON_STRONG_GRENADES);
    CHECK_EQ_U64(dummy_at(&e, 0)->as.dummy.hp, TUNING_DUMMY_HP);
    // On the same side it works.
    combat_explode(gs_of(&e), vec2(d->body.pos.x + 20.0f, d->body.pos.y + 12.0f), WEAPON_STRONG_GRENADES);
    CHECK(dummy_at(&e, 0)->as.dummy.hp < TUNING_DUMMY_HP);
    sim_free(&e.sim);
}

static void test_explosions_hurt_and_push_the_shooter(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 100.0f);
    const Vec2 pc = body_center(&player(&e)->body);
    combat_explode(gs_of(&e), vec2(pc.x - 15.0f, pc.y), WEAPON_ROCKET_LAUNCHER);
    CHECK(player(&e)->hp < TUNING_SOLDIER_HP);
    CHECK(player(&e)->body.vel.x > 50.0f); // pushed away from the blast (to the right)
    CHECK(player(&e)->body.vel.y < 0.0f);  // and lifted
    CHECK(player(&e)->hurt_ticks > 0);
    sim_free(&e.sim);
}

static void test_rocket_explodes_on_impact(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 40.0f);
    dummy_spawn(gs_of(&e), vec2(160.0f, FLOOR_Y), 1);
    combat_give_weapon(gs_of(&e), WEAPON_ROCKET_LAUNCHER);
    tick(&e, KEY(ACT_FIRE));
    const int t = ticks_until_event(&e, EV_EXPLOSION, 120);
    CHECK(t > 10 && t < 60); // 110 px at 220 px/s: about 30 ticks
    CHECK(dummy_at(&e, 0)->as.dummy.hp < TUNING_DUMMY_HP);
    CHECK_EQ_U64(player(&e)->hp, TUNING_SOLDIER_HP); // far enough
    CHECK_EQ_U64(gs_of(&e)->combat_stats.explosions, 1);
    tick_n(&e, 2);
    CHECK_EQ_U64(projectile_count(gs_of(&e)), 0);
    sim_free(&e.sim);
}

static void test_rocket_at_a_wall_explodes_on_the_free_side(void) {
    Env e;
    CHECK(env_init(&e, true, false));
    place_player(&e, 40.0f);
    combat_give_weapon(gs_of(&e), WEAPON_ROCKET_LAUNCHER);
    tick(&e, KEY(ACT_FIRE));
    CHECK(ticks_until_event(&e, EV_EXPLOSION, 120) > 0);
    const GameEvent *ev = &gs_of(&e)->events.items[0];
    for (u32 i = 0; i < gs_of(&e)->events.count; i++) {
        if (gs_of(&e)->events.items[i].type == EV_EXPLOSION) {
            ev = &gs_of(&e)->events.items[i];
        }
    }
    CHECK(ev->pos.x < 80.0f && ev->pos.x > 60.0f); // in front of the wall, not inside it
    sim_free(&e.sim);
}

static void test_grenade_is_lobbed_and_explodes_at_the_fuse(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 40.0f);
    combat_give_weapon(gs_of(&e), WEAPON_GRENADES);
    tick(&e, KEY(ACT_FIRE));
    // It goes up first.
    const Entity *g = NULL;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        if (gs_of(&e)->entities.slots[i].type == ENT_PROJECTILE) {
            g = &gs_of(&e)->entities.slots[i];
        }
    }
    CHECK(g != NULL && g->body.vel.y < 0.0f && g->body.vel.x > 0.0f);
    const int t = ticks_until_event(&e, EV_EXPLOSION, 200);
    CHECK_EQ_U64(t + 1, weapon_def(WEAPON_GRENADES)->life_ticks); // the fuse, counted from the throw
    CHECK_EQ_U64(projectile_count(gs_of(&e)), 0);
    CHECK_EQ_U64(player(&e)->hp, TUNING_SOLDIER_HP); // it landed far enough away
    sim_free(&e.sim);
}

static void test_grenade_explodes_on_contact_with_an_enemy(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 40.0f);
    dummy_spawn(gs_of(&e), vec2(112.0f, FLOOR_Y), 1);
    combat_give_weapon(gs_of(&e), WEAPON_GRENADES);
    tick(&e, KEY(ACT_FIRE));
    const int t = ticks_until_event(&e, EV_EXPLOSION, 200);
    CHECK(t > 0 && t < 50); // well before the fuse (90 ticks)
    CHECK(dummy_at(&e, 0)->as.dummy.hp < TUNING_DUMMY_HP);
    sim_free(&e.sim);
}

static void test_grenade_bounces_off_a_wall(void) {
    Env e;
    CHECK(env_init(&e, true, false)); // wall at x 80..96, up to y 96
    place_player(&e, 40.0f);
    combat_give_weapon(gs_of(&e), WEAPON_GRENADES);
    tick(&e, KEY(ACT_FIRE));
    f32 max_x = 0.0f;
    for (int i = 0; i < 80; i++) {
        tick(&e, 0);
        for (u16 s = 1; s <= MAX_ENTITIES; s++) {
            const Entity *p = &gs_of(&e)->entities.slots[s];
            if (p->type == ENT_PROJECTILE) {
                max_x = MAX(max_x, p->body.pos.x + p->body.size.x);
            }
        }
    }
    CHECK(max_x <= 80.0f + 0.01f); // never went through the wall
    sim_free(&e.sim);
}

// ---- Dummies and kills ---------------------------------------------------------------------------

static void test_dummy_dies_at_zero_and_is_removed(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 8.0f); // out of the blast
    dummy_spawn(gs_of(&e), vec2(100.0f, FLOOR_Y), 1);
    CHECK_EQ_U64(dummy_count(gs_of(&e)), 1);
    const Vec2 c = body_center(&dummy_at(&e, 0)->body);
    combat_explode(gs_of(&e), c, WEAPON_STRONG_GRENADES); // 100 damage at the center
    CHECK_EQ_U64(gs_of(&e)->combat_stats.kills, 1);
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_DUMMY_DESTROYED), 1);
    tick(&e, 0);
    CHECK_EQ_U64(count_type(&e, ENT_DUMMY), 0);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.kills, 1);
    // A dead dummy is not a target any more.
    combat_explode(gs_of(&e), c, WEAPON_STRONG_GRENADES);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.hits, 1);
    sim_free(&e.sim);
}

static void test_level_markers_create_dummies(void) {
    TestMap m;
    map_init(&m, 20, 12);
    map_rect(&m, 0, 10, 19, 11, '#');
    map_put(&m, 1, 9, '1');
    map_put(&m, 8, 9, 'T');
    map_put(&m, 12, 9, 'T');
    char text[4096];
    map_text(&m, text, sizeof(text));
    Sim sim;
    SimConfig cfg = {.level_text = text, .seed = 1};
    char err[128];
    CHECK(sim_init(&sim, &cfg, err, sizeof(err)));
    CHECK_EQ_U64(sim.level.dummy_count, 2);
    CHECK_EQ_U64(dummy_count(&sim.gs), 2);
    CHECK_NEAR(sim.level.dummies[0].x, 8 * 16 + 8, 0.0);
    CHECK_NEAR(sim.level.dummies[0].y, 160.0, 0.0);
    CHECK(level_tile(&sim.level, 8, 9) == TILE_EMPTY);
    sim_free(&sim);
}

static void test_enemy_bullets_hurt_the_player_but_not_their_own_team(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 100.0f);
    const Vec2 pc = body_center(&player(&e)->body);
    // A bullet of team 1 flying at the soldier: hurts and pushes.
    projectile_spawn(gs_of(&e), WEAPON_PISTOL, 1, vec2(pc.x - 20.0f, pc.y), vec2(360.0f, 0.0f));
    tick_n(&e, 10);
    CHECK_EQ_U64(player(&e)->hp, TUNING_SOLDIER_HP - weapon_def(WEAPON_PISTOL)->damage);
    // The player's own bullets pass through the player (same team).
    projectile_spawn(gs_of(&e), WEAPON_PISTOL, PLAYER_TEAM, vec2(pc.x - 20.0f, pc.y), vec2(360.0f, 0.0f));
    tick_n(&e, 10);
    CHECK_EQ_U64(player(&e)->hp, TUNING_SOLDIER_HP - weapon_def(WEAPON_PISTOL)->damage);
    sim_free(&e.sim);
}

// ---- Pickup --------------------------------------------------------------------------------------

static Vec2 under_player(Env *e) {
    const Body *b = &player(e)->body;
    return vec2(b->pos.x, b->pos.y + b->size.y - TUNING_LOOT_H);
}

static void test_unarmed_soldier_picks_up_a_weapon_by_walking_on_it(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 40.0f);
    loot_spawn_weapon(gs_of(&e), WEAPON_UZI, 33, vec2(100.0f, FLOOR_Y - TUNING_LOOT_H));
    tick_n(&e, 5);
    CHECK(player(&e)->weapon == WEAPON_FISTS); // not on it yet
    place_player(&e, 106.0f);
    tick(&e, 0);
    CHECK(player(&e)->weapon == WEAPON_UZI && player(&e)->ammo == 33);
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_WEAPON_PICKED_UP), 1);
    tick(&e, 0);
    CHECK_EQ_U64(count_type(&e, ENT_WEAPON), 0);
    sim_free(&e.sim);
}

static void test_armed_soldier_swaps_on_the_grab_key_only(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 100.0f);
    combat_give_weapon(gs_of(&e), WEAPON_PISTOL);
    player(&e)->ammo = 7;
    loot_spawn_weapon(gs_of(&e), WEAPON_ROCKET_LAUNCHER, 3, under_player(&e));
    tick_n(&e, 5);
    CHECK(player(&e)->weapon == WEAPON_PISTOL); // no automatic swap
    tick(&e, KEY(ACT_GRAB));
    CHECK(player(&e)->weapon == WEAPON_ROCKET_LAUNCHER && player(&e)->ammo == 3);
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_WEAPON_DROPPED), 1);
    tick(&e, 0);
    // The pistol with its 7 rounds now lies there.
    CHECK_EQ_U64(count_type(&e, ENT_WEAPON), 1);
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *w = &gs_of(&e)->entities.slots[i];
        if (w->type == ENT_WEAPON) {
            CHECK(w->as.weapon.kind == WEAPON_PISTOL && w->as.weapon.ammo == 7);
        }
    }
    sim_free(&e.sim);
}

static void test_a_crate_action_is_not_also_a_shot_or_a_swap(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 100.0f);
    combat_give_weapon(gs_of(&e), WEAPON_PISTOL);
    const Vec2 up = vec2(player(&e)->body.pos.x + 2.0f, player(&e)->body.pos.y - 16.0f);
    EntityId crate = crate_spawn(gs_of(&e), CRATE_RED, up, CRATE_CARRIED, -1);
    player(&e)->carrying = crate;
    loot_spawn_weapon(gs_of(&e), WEAPON_UZI, 5, under_player(&e));
    tick(&e, 0);
    // Fire while carrying throws the crate; it does not shoot.
    tick(&e, KEY(ACT_FIRE));
    CHECK_EQ_U64(gs_of(&e)->crate_stats.thrown, 1);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 0);
    CHECK_EQ_U64(player(&e)->ammo, 24);
    // Nor does the grab key that picks a crate up swap the weapon.
    sim_free(&e.sim);

    CHECK(env_init(&e, false, false));
    place_player(&e, 100.0f);
    combat_give_weapon(gs_of(&e), WEAPON_PISTOL);
    EntityId rest = crate_spawn(gs_of(&e), CRATE_RED, vec2(112.0f, FLOOR_Y - 16.0f), CRATE_RESTING, -1);
    CHECK(entity_id_valid(rest));
    loot_spawn_weapon(gs_of(&e), WEAPON_UZI, 5, under_player(&e));
    tick_n(&e, 3);
    tick(&e, KEY(ACT_GRAB));
    CHECK_EQ_U64(gs_of(&e)->crate_stats.picked_up, 1); // the crate wins
    CHECK(player(&e)->weapon == WEAPON_PISTOL);
    sim_free(&e.sim);
}

static void test_no_punching_while_carrying(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 100.0f);
    const Vec2 up = vec2(player(&e)->body.pos.x + 2.0f, player(&e)->body.pos.y - 16.0f);
    player(&e)->carrying = crate_spawn(gs_of(&e), CRATE_RED, up, CRATE_CARRIED, -1);
    dummy_spawn(gs_of(&e), vec2(113.0f, FLOOR_Y), 1); // within punching reach
    tick(&e, 0);
    tick(&e, KEY(ACT_FIRE)); // throws the crate...
    CHECK_EQ_U64(gs_of(&e)->crate_stats.thrown, 1);
    CHECK_EQ_U64(dummy_at(&e, 0)->as.dummy.hp, TUNING_DUMMY_HP); // ...and does not punch
    CHECK_EQ_U64(player(&e)->punch_ticks, 0);
    sim_free(&e.sim);
}

// ---- Death ---------------------------------------------------------------------------------------

static void test_death_drops_weapon_and_crate_then_respawns(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 200.0f);
    combat_give_weapon(gs_of(&e), WEAPON_UZI);
    player(&e)->ammo = 17;
    const Vec2 up = vec2(player(&e)->body.pos.x + 2.0f, player(&e)->body.pos.y - 16.0f);
    EntityId crate = crate_spawn(gs_of(&e), CRATE_BLUE, up, CRATE_CARRIED, -1);
    player(&e)->carrying = crate;
    tick(&e, 0);
    const Vec2 pc = body_center(&player(&e)->body);
    player(&e)->hp = 5;
    combat_explode(gs_of(&e), pc, WEAPON_GRENADES);
    CHECK(player(&e)->hp <= 0);
    tick(&e, 0);

    CHECK_EQ_U64(gs_of(&e)->combat_stats.deaths, 1);
    CHECK_EQ_U64(gs_of(&e)->respawns, 1);
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_PLAYER_DIED), 1);
    CHECK_EQ_U64(events_count_of(&gs_of(&e)->events, EV_PLAYER_RESPAWNED), 1);
    // Back at the spawn, healthy, with fists and empty hands.
    CHECK_EQ_U64(player(&e)->hp, TUNING_SOLDIER_HP);
    CHECK(player(&e)->weapon == WEAPON_FISTS);
    CHECK(!entity_id_valid(player(&e)->carrying));
    CHECK_NEAR(player(&e)->body.pos.x + player(&e)->body.size.x * 0.5f, 1 * 16 + 8, 0.01);
    // Its weapon (with the remaining ammo) and its crate stayed where it fell.
    CHECK_EQ_U64(count_type(&e, ENT_WEAPON), 1);
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *w = &gs_of(&e)->entities.slots[i];
        if (w->type == ENT_WEAPON) {
            CHECK(w->as.weapon.kind == WEAPON_UZI && w->as.weapon.ammo == 17);
            CHECK(w->body.pos.x > 150.0f);
        }
    }
    const Entity *c = pool_get(&gs_of(&e)->entities, crate);
    CHECK(c != NULL && c->as.crate.state != CRATE_CARRIED);
    CHECK_EQ_U64(crate_count(gs_of(&e)), 1);
    sim_free(&e.sim);
}

static void test_a_hit_makes_the_soldier_drop_the_crate(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 100.0f);
    const Vec2 up = vec2(player(&e)->body.pos.x + 2.0f, player(&e)->body.pos.y - 16.0f);
    EntityId crate = crate_spawn(gs_of(&e), CRATE_RED, up, CRATE_CARRIED, -1);
    player(&e)->carrying = crate;
    tick(&e, 0);
    CHECK(entity_id_valid(player(&e)->carrying));
    projectile_spawn(gs_of(&e), WEAPON_PISTOL, 1, vec2(80.0f, player(&e)->body.pos.y + 11.0f), vec2(360.0f, 0.0f));
    tick_n(&e, 10);
    CHECK(player(&e)->hp < TUNING_SOLDIER_HP);
    CHECK(!entity_id_valid(player(&e)->carrying));
    const Entity *c = pool_get(&gs_of(&e)->entities, crate);
    CHECK(c != NULL && c->as.crate.state != CRATE_CARRIED); // it fell, it was not destroyed
    sim_free(&e.sim);
}

static void test_dying_with_fists_drops_nothing(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 200.0f);
    player(&e)->hp = 1;
    combat_explode(gs_of(&e), body_center(&player(&e)->body), WEAPON_GRENADES);
    tick(&e, 0);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.deaths, 1);
    CHECK_EQ_U64(count_type(&e, ENT_WEAPON), 0);
    sim_free(&e.sim);
}

static void test_falling_in_the_water_loses_the_weapon(void) {
    TestMap m;
    map_init(&m, 20, 12);
    map_rect(&m, 0, 10, 19, 11, '#');
    map_rect(&m, 8, 10, 11, 11, '~');
    map_put(&m, 1, 9, '1');
    char text[4096];
    map_text(&m, text, sizeof(text));
    Sim sim;
    SimConfig cfg = {.level_text = text, .seed = 1};
    char err[128];
    CHECK(sim_init(&sim, &cfg, err, sizeof(err)));
    combat_give_weapon(&sim.gs, WEAPON_PISTOL);
    companion_init(&sim.gs.player, vec2(10 * 16.0f, 150.0f));
    InputFrame in = {0};
    for (int i = 0; i < 60; i++) {
        input_frame_update(&in, 0);
        sim_tick(&sim, &in);
    }
    CHECK_EQ_U64(sim.gs.respawns, 1);
    CHECK_EQ_U64(sim.gs.combat_stats.deaths, 0); // a hazard is not a death by damage
    CHECK(sim.gs.player.weapon == WEAPON_FISTS);
    u32 weapons = 0;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        weapons += sim.gs.entities.slots[i].type == ENT_WEAPON ? 1u : 0u;
    }
    CHECK_EQ_U64(weapons, 0);
    sim_free(&sim);
}

// ---- Pad -> weapon -------------------------------------------------------------------------------

static void test_a_pad_builds_a_real_weapon_that_can_be_used(void) {
    Env e;
    CHECK(env_init(&e, false, true)); // pad at columns 20-23
    CHECK(pad_debug_fill(gs_of(&e), 0, "....", "b..."));
    CHECK(pad_build(gs_of(&e), 0, SHAPE_1X1, true)); // blue 1x1: an uzi
    tick_n(&e, 60);
    CHECK_EQ_U64(count_type(&e, ENT_PRODUCT), 0);
    CHECK_EQ_U64(count_type(&e, ENT_WEAPON), 1);
    const Entity *w = NULL;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        if (gs_of(&e)->entities.slots[i].type == ENT_WEAPON) {
            w = &gs_of(&e)->entities.slots[i];
        }
    }
    CHECK(w != NULL && w->as.weapon.kind == WEAPON_UZI && w->as.weapon.ammo == 60);
    CHECK(w->body.on_ground);
    // Walk onto it: equipped.
    place_player(&e, w->body.pos.x + 6.0f);
    tick(&e, 0);
    CHECK(player(&e)->weapon == WEAPON_UZI && player(&e)->ammo == 60);
    sim_free(&e.sim);
}

static void test_soldier_and_vehicle_products_stay_placeholders(void) {
    Env e;
    CHECK(env_init(&e, false, true));
    CHECK(pad_debug_fill(gs_of(&e), 0, "r...", "r..."));
    CHECK(pad_build(gs_of(&e), 0, SHAPE_2X1_V, false)); // a companion: milestone 4
    tick_n(&e, 5);
    CHECK_EQ_U64(count_type(&e, ENT_PRODUCT), 1);
    CHECK_EQ_U64(count_type(&e, ENT_WEAPON), 0);
    sim_free(&e.sim);
}

// ---- Aiming with the mouse -----------------------------------------------------------------------

// A tick with the keys `mask` and the aim point (world pixels).
static void tick_aim(Env *e, u32 mask, f32 x, f32 y) {
    input_frame_update(&e->in, mask);
    e->in.aim_active = true;
    e->in.aim = vec2(x, y);
    sim_tick(&e->sim, &e->in);
    e->in.aim_active = false;
}

static const Entity *first_projectile(Env *e) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        if (gs_of(e)->entities.slots[i].type == ENT_PROJECTILE) {
            return &gs_of(e)->entities.slots[i];
        }
    }
    return NULL;
}

static void test_without_aim_the_weapon_points_along_the_facing(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 100.0f);
    tick(&e, KEY(ACT_LEFT));
    CHECK_NEAR(player(&e)->aim_dir.x, -1.0, 1e-6);
    CHECK_NEAR(player(&e)->aim_dir.y, 0.0, 1e-6);
    sim_free(&e.sim);
}

static void test_aim_gives_the_direction_and_turns_the_soldier(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 200.0f);
    tick(&e, 0);
    CHECK(player(&e)->facing == 1);
    const Vec2 sh = vec2(200.0f, player(&e)->body.pos.y + TUNING_MUZZLE_HEIGHT);
    // Cursor up and to the left: 45 degrees, and the soldier faces left without walking.
    tick_aim(&e, 0, sh.x - 50.0f, sh.y - 50.0f);
    CHECK(player(&e)->facing == -1);
    CHECK_NEAR(player(&e)->aim_dir.x, -0.7071, 0.001);
    CHECK_NEAR(player(&e)->aim_dir.y, -0.7071, 0.001);
    // Back to the keyboard: the weapon follows the facing again.
    tick(&e, 0);
    CHECK_NEAR(player(&e)->aim_dir.x, -1.0, 1e-6);
    CHECK_NEAR(player(&e)->aim_dir.y, 0.0, 1e-6);
    // A cursor right on the shoulder gives no direction: the facing is used.
    tick_aim(&e, 0, sh.x + 1.0f, sh.y);
    CHECK_NEAR(player(&e)->aim_dir.y, 0.0, 1e-6);
    sim_free(&e.sim);
}

static void test_bullets_fly_toward_the_cursor(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 100.0f);
    combat_give_weapon(gs_of(&e), WEAPON_PISTOL);
    tick(&e, 0);
    const Vec2 sh = vec2(100.0f, player(&e)->body.pos.y + TUNING_MUZZLE_HEIGHT);
    tick_aim(&e, KEY(ACT_FIRE), sh.x + 100.0f, sh.y - 100.0f);
    const Entity *b = first_projectile(&e);
    CHECK(b != NULL);
    if (b) {
        const f32 sp = weapon_def(WEAPON_PISTOL)->speed;
        CHECK_NEAR(b->body.vel.x, (double)sp * 0.7071, 0.5);
        CHECK_NEAR(b->body.vel.y, -(double)sp * 0.7071, 0.5);
    }
    sim_free(&e.sim);
}

static void test_aimed_shots_hit_what_is_under_the_cursor_and_miss_otherwise(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 40.0f);
    combat_give_weapon(gs_of(&e), WEAPON_PISTOL);
    dummy_spawn(gs_of(&e), vec2(200.0f, FLOOR_Y), 1);
    tick(&e, 0);
    const Vec2 dc = body_center(&dummy_at(&e, 0)->body);
    tick_aim(&e, KEY(ACT_FIRE), dc.x, dc.y); // slightly downward
    for (int i = 0; i < 60; i++) {
        tick(&e, 0);
    }
    CHECK(dummy_at(&e, 0)->as.dummy.hp < TUNING_DUMMY_HP);
    const i32 hp = dummy_at(&e, 0)->as.dummy.hp;
    tick_n(&e, 30);
    tick_aim(&e, KEY(ACT_FIRE), 300.0f, 20.0f); // way up: the bullet flies over
    tick_n(&e, 80);
    CHECK_EQ_U64(dummy_at(&e, 0)->as.dummy.hp, hp);
    sim_free(&e.sim);
}

static void test_uzi_spread_is_around_the_aim_direction(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 40.0f);
    combat_give_weapon(gs_of(&e), WEAPON_UZI);
    tick(&e, 0);
    const Vec2 sh = vec2(40.0f, player(&e)->body.pos.y + TUNING_MUZZLE_HEIGHT);
    const WeaponDef *d = weapon_def(WEAPON_UZI);
    for (int i = 0; i < 40; i++) {
        tick_aim(&e, KEY(ACT_FIRE), sh.x + 80.0f, sh.y - 80.0f);
        for (u16 s = 1; s <= MAX_ENTITIES; s++) {
            const Entity *p = &gs_of(&e)->entities.slots[s];
            if (p->type != ENT_PROJECTILE) {
                continue;
            }
            // Speed along the aim is the weapon's speed; the sideways part stays within the spread.
            const f32 along = (p->body.vel.x - p->body.vel.y) * 0.7071f;
            const f32 side = (p->body.vel.x + p->body.vel.y) * 0.7071f;
            CHECK_NEAR(along, d->speed, 1.0);
            CHECK(fabsf(side) <= d->spread * d->speed + 0.5f);
        }
    }
    sim_free(&e.sim);
}

static void test_grenade_lands_on_the_cursor(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 40.0f);
    combat_give_weapon(gs_of(&e), WEAPON_GRENADES);
    tick(&e, 0);
    for (f32 target = 90.0f; target <= 230.0f; target += 70.0f) {
        tick_n(&e, 60);
        place_player(&e, 40.0f); // (this resets the soldier: give the weapon again)
        combat_give_weapon(gs_of(&e), WEAPON_GRENADES);
        tick(&e, 0);
        tick_aim(&e, KEY(ACT_FIRE), target, FLOOR_Y);
        f32 landed_x = -1.0f;
        for (int i = 0; i < 80 && landed_x < 0.0f; i++) {
            tick(&e, 0);
            const Entity *g = first_projectile(&e);
            if (g && g->body.on_ground) {
                landed_x = g->body.pos.x + g->body.size.x * 0.5f;
            }
        }
        CHECK(landed_x > 0.0f);
        CHECK_NEAR(landed_x, target, 22.0);
        tick_n(&e, 100); // let it blow up
    }
    sim_free(&e.sim);
}

// Window position (logical pixels) of a world point, for the current camera.
static Vec2 to_logical(const Env *e, Vec2 world) {
    return vec2(world.x - roundf(e->sim.cam.pos.x), world.y - roundf(e->sim.cam.pos.y));
}

static void test_mouse_aims_and_fires_through_the_sim(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 100.0f);
    combat_give_weapon(gs_of(&e), WEAPON_PISTOL);
    tick(&e, 0);
    CHECK(!e.sim.mouse_active);
    // Mouse to the left of the soldier, button up: it turns to face it, nothing is fired.
    sim_set_mouse(&e.sim, to_logical(&e, vec2(20.0f, 100.0f)), false);
    tick(&e, 0);
    CHECK(player(&e)->facing == -1);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 0);
    // Button down: fires (one shot per press for a pistol), toward the cursor.
    sim_set_mouse(&e.sim, to_logical(&e, vec2(20.0f, 100.0f)), true);
    tick(&e, 0);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 1);
    CHECK(first_projectile(&e)->body.vel.x < 0.0f && first_projectile(&e)->body.vel.y < 0.0f);
    tick_n(&e, 40);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 1); // still held: no autofire for the pistol
    sim_free(&e.sim);
}

static void test_a_short_click_still_fires(void) {
    Env e;
    CHECK(env_init(&e, false, false));
    place_player(&e, 100.0f);
    combat_give_weapon(gs_of(&e), WEAPON_PISTOL);
    tick(&e, 0);
    sim_set_mouse(&e.sim, to_logical(&e, vec2(200.0f, 140.0f)), false);
    tick(&e, 0);
    // Pressed and released between two ticks: only the click event is seen.
    CHECK(!sim_click(&e.sim, to_logical(&e, vec2(200.0f, 140.0f))));
    tick(&e, 0);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 1);
    tick_n(&e, 40);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 1);
    sim_free(&e.sim);
}

static void test_clicking_the_pad_menu_does_not_fire(void) {
    Env e;
    CHECK(env_init(&e, false, true)); // pad at columns 20-23
    place_player(&e, 290.0f);
    combat_give_weapon(gs_of(&e), WEAPON_UZI);
    CHECK(pad_debug_fill(gs_of(&e), 0, "....", "rr.."));
    tick_n(&e, 30);
    PadMenu *m = &e.sim.menu;
    CHECK(m->visible && m->options.count > 0);
    const Rect r = m->rects[0];
    const Vec2 over = to_logical(&e, vec2(r.x + r.w * 0.5f, r.y + r.h * 0.5f));
    // Hovering with the button held: no fire.
    sim_set_mouse(&e.sim, over, true);
    tick_n(&e, 5);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 0);
    sim_set_mouse(&e.sim, over, false);
    tick(&e, 0);
    // The click builds the entry; the button still held afterwards must not shoot...
    CHECK(sim_click(&e.sim, over));
    sim_set_mouse(&e.sim, over, true);
    tick_n(&e, 5);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 0);
    // ...even if the menu is gone and the pointer is now over the world. Release, then fire.
    sim_set_mouse(&e.sim, to_logical(&e, vec2(500.0f, 100.0f)), true);
    tick_n(&e, 5);
    CHECK_EQ_U64(gs_of(&e)->combat_stats.shots, 0);
    sim_set_mouse(&e.sim, to_logical(&e, vec2(500.0f, 100.0f)), false);
    tick(&e, 0);
    sim_set_mouse(&e.sim, to_logical(&e, vec2(500.0f, 100.0f)), true);
    tick_n(&e, 5);
    CHECK(gs_of(&e)->combat_stats.shots > 0);
    sim_free(&e.sim);
}

// ---- Bots ----------------------------------------------------------------------------------------

static void check_combat_invariants(const Sim *sim) {
    const GameState *gs = &sim->gs;
    const Companion *p = &gs->player;
    CHECK(p->hp > 0 && p->hp <= p->max_hp); // a dead soldier is respawned within the same tick
    CHECK(p->weapon >= WEAPON_FISTS && p->weapon < WEAPON_COUNT);
    if (p->weapon == WEAPON_FISTS) {
        CHECK_EQ_U64(p->ammo, 0);
    } else {
        CHECK(p->ammo > 0 && p->ammo <= weapon_def(p->weapon)->max_ammo);
    }
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type == ENT_DUMMY) {
            CHECK(e->as.dummy.hp > 0 && e->as.dummy.hp <= e->as.dummy.max_hp);
        } else if (e->type == ENT_PROJECTILE) {
            const WeaponDef *d = weapon_def(e->as.projectile.weapon);
            CHECK(e->as.projectile.ttl > 0 && e->as.projectile.ttl <= d->life_ticks);
            if (d->shot != SHOT_GRENADE) { // straight shots are never left inside terrain
                const Vec2 c = body_center(&e->body);
                CHECK(!level_is_solid(&sim->level, level_px_to_tile(c.x), level_px_to_tile(c.y)));
            }
        } else if (e->type == ENT_WEAPON) {
            CHECK(e->as.weapon.ammo > 0 && e->as.weapon.ammo <= weapon_def(e->as.weapon.kind)->max_ammo);
        }
    }
    CHECK(gs->combat_stats.kills <= 3); // the arena has three dummies, none respawns
    CHECK(gs->combat_stats.hits >= gs->combat_stats.kills);
}

static u64 run_bot_with_weapons(u64 seed, u32 ticks, u32 *shots, u32 *hits) {
    Sim sim;
    char path[512];
    snprintf(path, sizeof(path), "%s/levels/test_arena.lvl", CS_ASSET_DIR);
    SimConfig cfg = {.level_path = path, .seed = seed, .use_bot = true};
    char err[256] = "";
    if (!sim_init(&sim, &cfg, err, sizeof(err))) {
        fprintf(stderr, "sim_init failed: %s\n", err);
        g_failures++;
        return 0;
    }
    CHECK_EQ_U64(dummy_count(&sim.gs), 3);
    // Walks the bot to the dummies: it starts next to them with a fresh weapon every few seconds.
    for (u32 t = 0; t < ticks; t++) {
        if (t % 240 == 0) {
            combat_give_weapon(&sim.gs, (WeaponKind)(1 + (t / 240) % (WEAPON_COUNT - 1)));
        }
        if (t % 480 == 0) {
            companion_init(&sim.gs.player, vec2(430.0f + (f32)(t % 3) * 60.0f, 432.0f));
            combat_give_weapon(&sim.gs, (WeaponKind)(1 + (t / 480) % (WEAPON_COUNT - 1)));
        }
        sim_tick(&sim, NULL);
        check_combat_invariants(&sim);
    }
    const u64 hash = game_hash(&sim.gs);
    *shots = sim.gs.combat_stats.shots;
    *hits = sim.gs.combat_stats.hits;
    sim_free(&sim);
    return hash;
}

static void test_bot_fights_keep_the_invariants_and_are_deterministic(void) {
    u32 shots_a = 0, hits_a = 0, shots_b = 0, hits_b = 0;
    const u64 a = run_bot_with_weapons(5, 6000, &shots_a, &hits_a);
    const u64 b = run_bot_with_weapons(5, 6000, &shots_b, &hits_b);
    CHECK(a == b);
    CHECK_EQ_U64(shots_a, shots_b);
    CHECK_EQ_U64(hits_a, hits_b);
    CHECK(shots_a > 20); // the bot really did shoot
    const u64 c = run_bot_with_weapons(6, 6000, &shots_b, &hits_b);
    CHECK(a != c);
}

int main(void) {
    RUN_TEST(test_weapon_table_is_sane);
    RUN_TEST(test_pad_products_map_to_weapons);
    RUN_TEST(test_pistol_shoots_one_bullet_per_press);
    RUN_TEST(test_bullet_hits_a_dummy_and_pushes_nothing_else);
    RUN_TEST(test_bullet_stops_at_a_wall);
    RUN_TEST(test_bullet_range_ends);
    RUN_TEST(test_shooting_left);
    RUN_TEST(test_uzi_is_automatic_and_spreads);
    RUN_TEST(test_running_out_of_ammo_goes_back_to_fists);
    RUN_TEST(test_empty_event_is_sent_with_the_last_shot);
    RUN_TEST(test_fists_hit_within_reach_only);
    RUN_TEST(test_fists_infinite);
    RUN_TEST(test_explosion_damage_falls_off_with_distance);
    RUN_TEST(test_stronger_explosions_hurt_more_and_reach_further);
    RUN_TEST(test_walls_block_explosions);
    RUN_TEST(test_explosions_hurt_and_push_the_shooter);
    RUN_TEST(test_rocket_explodes_on_impact);
    RUN_TEST(test_rocket_at_a_wall_explodes_on_the_free_side);
    RUN_TEST(test_grenade_is_lobbed_and_explodes_at_the_fuse);
    RUN_TEST(test_grenade_explodes_on_contact_with_an_enemy);
    RUN_TEST(test_grenade_bounces_off_a_wall);
    RUN_TEST(test_dummy_dies_at_zero_and_is_removed);
    RUN_TEST(test_level_markers_create_dummies);
    RUN_TEST(test_enemy_bullets_hurt_the_player_but_not_their_own_team);
    RUN_TEST(test_unarmed_soldier_picks_up_a_weapon_by_walking_on_it);
    RUN_TEST(test_armed_soldier_swaps_on_the_grab_key_only);
    RUN_TEST(test_a_crate_action_is_not_also_a_shot_or_a_swap);
    RUN_TEST(test_no_punching_while_carrying);
    RUN_TEST(test_death_drops_weapon_and_crate_then_respawns);
    RUN_TEST(test_a_hit_makes_the_soldier_drop_the_crate);
    RUN_TEST(test_dying_with_fists_drops_nothing);
    RUN_TEST(test_falling_in_the_water_loses_the_weapon);
    RUN_TEST(test_a_pad_builds_a_real_weapon_that_can_be_used);
    RUN_TEST(test_soldier_and_vehicle_products_stay_placeholders);
    RUN_TEST(test_without_aim_the_weapon_points_along_the_facing);
    RUN_TEST(test_aim_gives_the_direction_and_turns_the_soldier);
    RUN_TEST(test_bullets_fly_toward_the_cursor);
    RUN_TEST(test_aimed_shots_hit_what_is_under_the_cursor_and_miss_otherwise);
    RUN_TEST(test_uzi_spread_is_around_the_aim_direction);
    RUN_TEST(test_grenade_lands_on_the_cursor);
    RUN_TEST(test_mouse_aims_and_fires_through_the_sim);
    RUN_TEST(test_a_short_click_still_fires);
    RUN_TEST(test_clicking_the_pad_menu_does_not_fire);
    RUN_TEST(test_bot_fights_keep_the_invariants_and_are_deterministic);
    TEST_MAIN_END();
}
