// The F2 spawn menu: its layout, what each entry puts on the map and where, and how clicks reach it through the Sim.
#include "test.h"

#include "app/debug_menu.h"
#include "app/sim.h"
#include "game/combat.h"
#include "game/crate.h"
#include "game/loot.h"
#include "game/tuning.h"
#include "test_helpers.h"

#include <math.h>

static Sim g_sim;

// 24x12 map: floor on rows 10-11 (top at y = 160), the player spawns at column 6 (a wall at column 11 stops the way).
static bool start(bool wall) {
    TestMap m;
    map_init(&m, 24, 12);
    map_rect(&m, 0, 10, 23, 11, '#');
    if (wall) {
        map_rect(&m, 11, 6, 11, 9, '#');
    }
    map_put(&m, 6, 9, '1');
    char text[4096];
    map_text(&m, text, sizeof(text));
    const SimConfig cfg = {.level_text = text, .seed = 1};
    char err[256] = "";
    if (!sim_init(&g_sim, &cfg, err, sizeof(err))) {
        return false;
    }
    InputFrame in = {0};
    for (int t = 0; t < 30; t++) { // settle on the floor
        input_frame_update(&in, 0);
        sim_tick(&g_sim, &in);
    }
    return true;
}

static void tick_n(int n) {
    InputFrame in = {0};
    for (int t = 0; t < n; t++) {
        input_frame_update(&in, 0);
        sim_tick(&g_sim, &in);
    }
}

static u32 count_type(EntityType type) {
    u32 n = 0;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        n += g_sim.gs.entities.slots[i].type == type && !g_sim.gs.entities.slots[i].pending_destroy ? 1u : 0u;
    }
    return n;
}

static const Entity *first_of(EntityType type) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        if (g_sim.gs.entities.slots[i].type == type) {
            return &g_sim.gs.entities.slots[i];
        }
    }
    return NULL;
}

static Vec2 center_of(Rect r) {
    return vec2(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
}

static void test_the_layout_fits_the_screen_and_every_entry_is_clickable(void) {
    DebugMenu menu = {.visible = true};
    const Rect panel = debug_menu_panel();
    CHECK(panel.x >= 0.0f && panel.y >= 0.0f && panel.x + panel.w <= (f32)LOGICAL_W && panel.y + panel.h <= (f32)LOGICAL_H);
    u32 groups = 0;
    for (int i = 0; i < DEBUG_ITEM_COUNT; i++) {
        const Rect r = debug_menu_item_rect((DebugItem)i);
        CHECK(r.w > 0.0f && r.h > 0.0f);
        CHECK(r.x >= panel.x && r.y >= panel.y && r.x + r.w <= panel.x + panel.w && r.y + r.h <= panel.y + panel.h);
        CHECK(debug_menu_hit_test(&menu, center_of(r)) == i);
        CHECK(debug_menu_covers(&menu, center_of(r)));
        CHECK(debug_item_label((DebugItem)i)[0] != '\0');
        groups += debug_item_group((DebugItem)i) ? 1u : 0u;
        if (i > 0) { // entries do not overlap, and go down the list
            const Rect before = debug_menu_item_rect((DebugItem)(i - 1));
            CHECK(r.y >= before.y + before.h);
        }
    }
    CHECK_EQ_U64(groups, 4); // crates, allies, weapons, vehicles
    CHECK(debug_item_group(DEBUG_CRATE_RED) && !debug_item_group(DEBUG_CRATE_BLUE));
    // Hidden: nothing is hit and nothing is covered. The panel's margin is covered but hits no entry.
    CHECK(debug_menu_hit_test(&(DebugMenu){0}, center_of(debug_menu_item_rect(DEBUG_WEAPON_UZI))) == -1);
    CHECK(!debug_menu_covers(&(DebugMenu){0}, center_of(panel)));
    CHECK(debug_menu_covers(&menu, vec2(panel.x + 1.0f, panel.y + 1.0f)));
    CHECK(debug_menu_hit_test(&menu, vec2(panel.x + 1.0f, panel.y + 1.0f)) == -1);
    CHECK(!debug_menu_covers(&menu, vec2(10.0f, 10.0f)));
}

static void test_crates_appear_above_the_soldier_and_fall(void) {
    CHECK(start(false));
    CHECK(sim_debug_spawn(&g_sim, DEBUG_CRATE_RED));
    CHECK(sim_debug_spawn(&g_sim, DEBUG_CRATE_BLUE));
    CHECK_EQ_U64(count_type(ENT_CRATE), 2);
    const Body *me = &game_player_const(&g_sim.gs)->body;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &g_sim.gs.entities.slots[i];
        if (e->type == ENT_CRATE) {
            CHECK(e->as.crate.state == CRATE_FALLING);
            CHECK(e->body.pos.y < me->pos.y); // above the head: it comes down
            CHECK(fabsf(e->body.pos.x - me->pos.x) < 80.0f);
            CHECK(e->body.pos.x + 16.0f <= me->pos.x || e->body.pos.x >= me->pos.x + me->size.x); // not onto the soldier
        }
    }
    tick_n(90);
    u32 resting = 0;
    CHECK_EQ_U64(crate_count(&g_sim.gs), 2);
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &g_sim.gs.entities.slots[i];
        resting += e->type == ENT_CRATE && e->as.crate.state == CRATE_RESTING ? 1u : 0u;
    }
    CHECK_EQ_U64(resting, 2);
    sim_free(&g_sim);
}

static void test_an_ally_joins_the_team_beside_the_soldier(void) {
    CHECK(start(false));
    const u32 before = team_soldier_count(&g_sim.gs, PLAYER_TEAM);
    for (int v = 0; v < 4; v++) {
        CHECK(sim_debug_spawn(&g_sim, (DebugItem)(DEBUG_ALLY_NORMAL + v)));
    }
    CHECK_EQ_U64(team_soldier_count(&g_sim.gs, PLAYER_TEAM), before + 4);
    const Body *me = &game_player_const(&g_sim.gs)->body;
    u32 seen[4] = {0};
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        const Soldier *s = &g_sim.gs.soldiers[i];
        if (!s->alive || i == g_sim.gs.controlled) {
            continue;
        }
        CHECK_EQ_U64(s->team, PLAYER_TEAM);
        CHECK(s->variant >= 0 && s->variant < 4);
        seen[s->variant]++;
        CHECK(fabsf(s->c.body.pos.x - me->pos.x) >= 20.0f); // not on top of the soldier
        CHECK(fabsf((s->c.body.pos.y + s->c.body.size.y) - (me->pos.y + me->size.y)) < 1.0f); // standing on the floor
    }
    CHECK(seen[0] == 1 && seen[1] == 1 && seen[2] == 1 && seen[3] == 1);
    sim_free(&g_sim);
}

static void test_weapons_and_vehicles_land_on_the_ground(void) {
    CHECK(start(false));
    const DebugItem weapons[] = {DEBUG_WEAPON_PISTOL, DEBUG_WEAPON_UZI, DEBUG_WEAPON_GRENADES, DEBUG_WEAPON_STRONG_GRENADES,
                                 DEBUG_WEAPON_ROCKET_LAUNCHER};
    const WeaponKind kinds[] = {WEAPON_PISTOL, WEAPON_UZI, WEAPON_GRENADES, WEAPON_STRONG_GRENADES, WEAPON_ROCKET_LAUNCHER};
    for (int i = 0; i < 5; i++) {
        const u32 before = count_type(ENT_WEAPON);
        CHECK(sim_debug_spawn(&g_sim, weapons[i]));
        CHECK_EQ_U64(count_type(ENT_WEAPON), before + 1);
        // The newest weapon is the one with the highest slot among its kind.
        const Entity *found = NULL;
        for (u16 s = 1; s <= MAX_ENTITIES; s++) {
            const Entity *e = &g_sim.gs.entities.slots[s];
            found = e->type == ENT_WEAPON && e->as.weapon.kind == kinds[i] ? e : found;
        }
        CHECK(found && found->as.weapon.ammo == weapon_def(kinds[i])->max_ammo); // a full weapon
    }
    CHECK(sim_debug_spawn(&g_sim, DEBUG_VEHICLE_TANK));
    CHECK(sim_debug_spawn(&g_sim, DEBUG_VEHICLE_HEAVY_TANK));
    CHECK_EQ_U64(count_type(ENT_PRODUCT), 2);
    const Entity *tank = first_of(ENT_PRODUCT);
    CHECK(tank && tank->as.product.kind == PRODUCT_TANK && tank->as.product.team == PLAYER_TEAM);
    tick_n(90); // everything came down to the floor (top at y = 160)
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &g_sim.gs.entities.slots[i];
        if (e->type == ENT_WEAPON || e->type == ENT_PRODUCT) {
            CHECK_NEAR(e->body.pos.y + e->body.size.y, 160.0, 1.0);
        }
    }
    sim_free(&g_sim);
}

static void test_the_other_side_is_used_when_the_facing_side_is_blocked(void) {
    CHECK(start(true)); // a wall at column 11 (x = 176..192), the soldier at column 6 faces right
    Companion *p = game_player(&g_sim.gs);
    p->body.pos.x = 11.0f * 16.0f - 14.0f; // right against the wall
    p->prev_pos = p->body.pos;
    p->facing = 1;
    CHECK(sim_debug_spawn(&g_sim, DEBUG_ALLY_NORMAL));
    const Soldier *ally = NULL;
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        ally = g_sim.gs.soldiers[i].alive && i != g_sim.gs.controlled ? &g_sim.gs.soldiers[i] : ally;
    }
    CHECK(ally && ally->c.body.pos.x < p->body.pos.x); // behind: the wall is in the way in front
    sim_free(&g_sim);
}

static void test_nothing_is_spawned_without_room(void) {
    CHECK(start(false));
    // Every free tile becomes rock, except the ones the soldier itself occupies: nowhere to put anything.
    const Body *me = &game_player_const(&g_sim.gs)->body;
    const i32 x0 = level_px_to_tile(me->pos.x), x1 = level_px_to_tile(me->pos.x + me->size.x - 0.01f);
    const i32 y0 = level_px_to_tile(me->pos.y), y1 = level_px_to_tile(me->pos.y + me->size.y - 0.01f);
    for (i32 ty = 0; ty < 12; ty++) {
        for (i32 tx = 0; tx < 24; tx++) {
            const bool mine = tx >= x0 && tx <= x1 && ty >= y0 && ty <= y1;
            if (!mine && !level_is_solid(&g_sim.level, tx, ty)) {
                level_set_dyn_solid(&g_sim.level, tx, ty, true);
            }
        }
    }
    const u32 soldiers = team_soldier_count(&g_sim.gs, PLAYER_TEAM);
    CHECK(!sim_debug_spawn(&g_sim, DEBUG_ALLY_NORMAL));
    CHECK(!sim_debug_spawn(&g_sim, DEBUG_CRATE_RED));
    CHECK(!sim_debug_spawn(&g_sim, DEBUG_WEAPON_UZI));
    CHECK(!sim_debug_spawn(&g_sim, DEBUG_VEHICLE_TANK));
    CHECK_EQ_U64(team_soldier_count(&g_sim.gs, PLAYER_TEAM), soldiers);
    CHECK_EQ_U64(count_type(ENT_CRATE) + count_type(ENT_WEAPON) + count_type(ENT_PRODUCT), 0);
    sim_free(&g_sim);
}

static void test_clicks_reach_the_menu_through_the_sim(void) {
    CHECK(start(false));
    CHECK(!g_sim.debug.visible);
    // Hidden: a click on that spot is an ordinary click.
    const Vec2 spot = center_of(debug_menu_item_rect(DEBUG_CRATE_BLUE));
    CHECK(!sim_click(&g_sim, spot));
    CHECK_EQ_U64(count_type(ENT_CRATE), 0);
    sim_toggle_debug_menu(&g_sim);
    CHECK(g_sim.debug.visible);
    CHECK(sim_click(&g_sim, spot));
    CHECK_EQ_U64(count_type(ENT_CRATE), 1);
    CHECK(first_of(ENT_CRATE)->as.crate.kind == CRATE_BLUE);
    CHECK(g_sim.mouse_fire_blocked); // the press belongs to the menu: the soldier does not fire
    // On the panel but not on an entry: nothing spawns, and it still does not fire.
    g_sim.mouse_fire_blocked = false;
    const Rect panel = debug_menu_panel();
    CHECK(!sim_click(&g_sim, vec2(panel.x + 1.0f, panel.y + 1.0f)));
    CHECK(g_sim.mouse_fire_blocked && count_type(ENT_CRATE) == 1);
    // Away from the panel: a normal click (a shot), nothing spawned.
    g_sim.mouse_fire_blocked = false;
    CHECK(!sim_click(&g_sim, vec2(50.0f, 50.0f)));
    CHECK(g_sim.mouse_click_pending && !g_sim.mouse_fire_blocked);
    sim_toggle_debug_menu(&g_sim);
    CHECK(!g_sim.debug.visible);
    sim_free(&g_sim);
}

static void test_the_mouse_over_the_menu_does_not_fire(void) {
    CHECK(start(false));
    combat_give_weapon(&g_sim.gs, WEAPON_PISTOL);
    sim_toggle_debug_menu(&g_sim);
    const Vec2 on_panel = center_of(debug_menu_item_rect(DEBUG_WEAPON_UZI));
    sim_set_mouse(&g_sim, on_panel, true, true); // both buttons held over the menu
    tick_n(5);
    CHECK_EQ_U64(g_sim.gs.combat_stats.shots, 0);
    sim_set_mouse(&g_sim, vec2(60.0f, 60.0f), true, false); // away from it: the pistol fires
    tick_n(2);
    CHECK(g_sim.gs.combat_stats.shots >= 1);
    sim_free(&g_sim);
}

static void test_with_nobody_to_follow_things_appear_in_the_middle_of_the_screen(void) {
    CHECK(start(false));
    g_sim.gs.controlled = -1; // spectating
    CHECK(sim_debug_spawn(&g_sim, DEBUG_WEAPON_UZI));
    const Entity *w = first_of(ENT_WEAPON);
    const f32 mid_x = roundf(g_sim.cam.pos.x) + (f32)LOGICAL_W * 0.5f;
    CHECK(w && fabsf(w->body.pos.x + w->body.size.x * 0.5f - mid_x) < 70.0f);
    sim_free(&g_sim);
}

int main(void) {
    RUN_TEST(test_the_layout_fits_the_screen_and_every_entry_is_clickable);
    RUN_TEST(test_crates_appear_above_the_soldier_and_fall);
    RUN_TEST(test_an_ally_joins_the_team_beside_the_soldier);
    RUN_TEST(test_weapons_and_vehicles_land_on_the_ground);
    RUN_TEST(test_the_other_side_is_used_when_the_facing_side_is_blocked);
    RUN_TEST(test_nothing_is_spawned_without_room);
    RUN_TEST(test_clicks_reach_the_menu_through_the_sim);
    RUN_TEST(test_the_mouse_over_the_menu_does_not_fire);
    RUN_TEST(test_with_nobody_to_follow_things_appear_in_the_middle_of_the_screen);
    TEST_MAIN_END();
}
