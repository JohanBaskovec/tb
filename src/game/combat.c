#include "game/combat.h"

#include "core/log.h"
#include "game/crate.h"
#include "game/loot.h"
#include "game/tuning.h"

#include <math.h>

#define HURT_FLASH_TICKS SECONDS_TO_TICKS(TUNING_HURT_FLASH_S)
#define PUNCH_ANIM_TICKS SECONDS_TO_TICKS(TUNING_PUNCH_ANIM_S)

// ---- Targets -----------------------------------------------------------------------------------

typedef struct Target {
    bool is_soldier;
    u16 slot; // soldier index, or dummy entity slot
    i32 team;
    Rect rect;
} Target;

#define MAX_TARGETS (MAX_ENTITIES + 1)

static EntityId slot_id(const GameState *gs, u16 slot) {
    return (EntityId){slot, gs->entities.slots[slot].generation};
}

static u32 collect_targets(GameState *gs, Target *out) {
    u32 n = 0;
    for (u16 i = 0; i < MAX_SOLDIERS; i++) {
        const Soldier *s = &gs->soldiers[i];
        if (s->alive && s->c.hp > 0) {
            out[n++] = (Target){true, i, s->team, body_rect(&s->c.body)};
        }
    }
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type == ENT_DUMMY && !e->pending_destroy && e->as.dummy.hp > 0) {
            out[n++] = (Target){false, i, e->as.dummy.team, body_rect(&e->body)};
        }
    }
    return n;
}

static Vec2 rect_center(Rect r) {
    return vec2(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
}

static void hurt(GameState *gs, const Target *t, i32 amount, Vec2 impulse) {
    if (amount <= 0) {
        return;
    }
    gs->combat_stats.hits++;
    events_push(&gs->events, EV_DAMAGE, rect_center(t->rect), amount, t->is_soldier ? 1 : 0);
    if (t->is_soldier) {
        Companion *p = &gs->soldiers[t->slot].c;
        p->hp -= amount;
        p->hurt_ticks = HURT_FLASH_TICKS;
        crate_release_carried(gs, p); // a hit makes the soldier drop what it carries
        p->body.vel = vec2_add(p->body.vel, impulse);
        if (impulse.y < 0.0f) {
            p->body.on_ground = false;
        }
        return;
    }
    Entity *e = &gs->entities.slots[t->slot];
    e->as.dummy.hp -= amount;
    e->as.dummy.hurt_ticks = HURT_FLASH_TICKS;
    if (e->as.dummy.hp <= 0) {
        gs->combat_stats.kills++;
        events_push(&gs->events, EV_DUMMY_DESTROYED, body_center(&e->body), 0, 0);
        pool_mark_destroy(&gs->entities, slot_id(gs, t->slot));
    }
}

// ---- Geometry ----------------------------------------------------------------------------------

static bool rect_hits_solid(const Level *lv, Rect r) {
    const i32 tx0 = level_px_to_tile(r.x + 0.001f), tx1 = level_px_to_tile(r.x + r.w - 0.001f);
    const i32 ty0 = level_px_to_tile(r.y + 0.001f), ty1 = level_px_to_tile(r.y + r.h - 0.001f);
    for (i32 ty = ty0; ty <= ty1; ty++) {
        for (i32 tx = tx0; tx <= tx1; tx++) {
            if (level_is_solid(lv, tx, ty)) {
                return true;
            }
        }
    }
    return false;
}

// True if no solid tile lies on the segment a-b.
bool combat_line_clear(const Level *lv, Vec2 a, Vec2 b) {
    const f32 dx = b.x - a.x, dy = b.y - a.y;
    const f32 len = sqrtf(dx * dx + dy * dy);
    const i32 steps = (i32)(len / 2.0f) + 1;
    for (i32 i = 0; i <= steps; i++) {
        const f32 t = (f32)i / (f32)steps;
        if (level_is_solid(lv, level_px_to_tile(a.x + dx * t), level_px_to_tile(a.y + dy * t))) {
            return false;
        }
    }
    return true;
}

// ---- Explosions --------------------------------------------------------------------------------

void combat_explode(GameState *gs, Vec2 center, WeaponKind weapon) {
    const WeaponDef *def = weapon_def(weapon);
    gs->combat_stats.explosions++;
    events_push(&gs->events, EV_EXPLOSION, center, (i32)def->radius, (i32)weapon);

    Target targets[MAX_TARGETS];
    const u32 n = collect_targets(gs, targets);
    for (u32 i = 0; i < n; i++) {
        const Target *t = &targets[i];
        // Distance from the center of the blast to the closest point of the target.
        const Vec2 near = vec2(clampf(center.x, t->rect.x, t->rect.x + t->rect.w),
                               clampf(center.y, t->rect.y, t->rect.y + t->rect.h));
        const f32 dx = near.x - center.x, dy = near.y - center.y;
        const f32 dist = sqrtf(dx * dx + dy * dy);
        if (dist > def->radius || !combat_line_clear(gs->level, center, near)) {
            continue;
        }
        const f32 falloff = 1.0f - (1.0f - TUNING_EXPLOSION_MIN_FALLOFF) * (dist / def->radius);
        // Push away from the blast, with a bias upwards so that it lifts things off the ground.
        const Vec2 tc = rect_center(t->rect);
        f32 px = tc.x - center.x, py = tc.y - center.y - 10.0f;
        const f32 len = sqrtf(px * px + py * py);
        if (len < 0.001f) {
            px = 0.0f;
            py = -1.0f;
        } else {
            px /= len;
            py /= len;
        }
        const Vec2 impulse = vec2(px * def->knockback * falloff,
                                  py * def->knockback * falloff - TUNING_EXPLOSION_LIFT * falloff);
        hurt(gs, t, (i32)((f32)def->damage * falloff + 0.5f), impulse);
    }
}

// ---- Projectiles -------------------------------------------------------------------------------

EntityId projectile_spawn(GameState *gs, WeaponKind weapon, i32 team, Vec2 center, Vec2 vel) {
    const WeaponDef *def = weapon_def(weapon);
    EntityId id = pool_create(&gs->entities, ENT_PROJECTILE);
    if (!entity_id_valid(id)) {
        return ENTITY_NONE;
    }
    Entity *e = pool_get(&gs->entities, id);
    e->body.size = vec2(def->proj_w, def->proj_h);
    e->body.pos = vec2(center.x - def->proj_w * 0.5f, center.y - def->proj_h * 0.5f);
    e->body.vel = vel;
    e->prev_pos = e->body.pos;
    e->as.projectile = (Projectile){.weapon = weapon, .team = team, .ttl = def->life_ticks};
    return id;
}

// The projectile is over: bullets vanish, rockets and grenades blow up. `victim` (may be NULL) is the
// target a bullet hit.
static void projectile_end(GameState *gs, u16 slot, const Target *victim) {
    Entity *e = &gs->entities.slots[slot];
    const WeaponDef *def = weapon_def(e->as.projectile.weapon);
    if (def->radius > 0.0f) {
        combat_explode(gs, body_center(&e->body), e->as.projectile.weapon);
    } else {
        events_push(&gs->events, EV_PROJECTILE_HIT, body_center(&e->body), (i32)e->as.projectile.weapon, 0);
        if (victim) {
            const f32 dir = e->body.vel.x >= 0.0f ? 1.0f : -1.0f;
            hurt(gs, victim, def->damage, vec2(dir * def->knockback, 0.0f));
        }
    }
    pool_mark_destroy(&gs->entities, slot_id(gs, slot));
}

static const Target *hostile_hit(const Target *targets, u32 n, Rect r, i32 team) {
    for (u32 i = 0; i < n; i++) {
        if (targets[i].team != team && rect_overlap(r, targets[i].rect)) {
            return &targets[i];
        }
    }
    return NULL;
}

static void projectile_update(GameState *gs, u16 slot, const Target *targets, u32 n) {
    Entity *e = &gs->entities.slots[slot];
    const ShotKind shot = weapon_def(e->as.projectile.weapon)->shot;
    e->prev_pos = e->body.pos;
    if (e->as.projectile.ttl > 0) {
        e->as.projectile.ttl--;
    }

    if (shot == SHOT_GRENADE) {
        const f32 vx = e->body.vel.x, vy = e->body.vel.y;
        e->body.vel.y = MIN(vy + TUNING_GRAVITY * TICK_DT, TUNING_MAX_FALL_SPEED);
        body_move(&e->body, gs->level, TICK_DT, false);
        if (e->body.hit_wall) {
            e->body.vel.x = -vx * TUNING_GRENADE_WALL_BOUNCE;
        }
        if (e->body.on_ground) {
            e->body.vel.y = vy > 80.0f ? -vy * TUNING_GRENADE_BOUNCE : 0.0f;
            e->body.vel.x *= TUNING_GRENADE_FRICTION;
        }
        if (e->body.pos.y > level_height_px(gs->level) + (f32)(2 * TILE_SIZE) ||
            body_overlaps_tile(&e->body, gs->level, TILE_HAZARD)) {
            pool_mark_destroy(&gs->entities, slot_id(gs, slot)); // lost in the water / the void
            return;
        }
    } else {
        e->body.pos = vec2_add(e->body.pos, vec2_scale(e->body.vel, TICK_DT));
        if (rect_hits_solid(gs->level, body_rect(&e->body))) {
            e->body.pos = e->prev_pos; // end the flight where it last was free
            projectile_end(gs, slot, NULL);
            return;
        }
    }

    const Target *victim = hostile_hit(targets, n, body_rect(&e->body), e->as.projectile.team);
    if (victim || e->as.projectile.ttl == 0) {
        projectile_end(gs, slot, victim);
    }
}

void combat_update(GameState *gs) {
    // Targets are read once: a projectile killing a dummy does not stop the others from hitting it
    // the same tick (it is simply destroyed at the end of the tick).
    Target targets[MAX_TARGETS];
    u32 n = collect_targets(gs, targets);
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        Entity *e = &gs->entities.slots[i];
        if (e->pending_destroy) {
            continue;
        }
        if (e->type == ENT_PROJECTILE) {
            projectile_update(gs, i, targets, n);
        } else if (e->type == ENT_DUMMY) {
            e->prev_pos = e->body.pos;
            if (e->as.dummy.hurt_ticks > 0) {
                e->as.dummy.hurt_ticks--;
            }
        }
    }
}

// ---- Dummies -----------------------------------------------------------------------------------

EntityId dummy_spawn(GameState *gs, Vec2 feet, i32 team) {
    EntityId id = pool_create(&gs->entities, ENT_DUMMY);
    if (!entity_id_valid(id)) {
        return ENTITY_NONE;
    }
    Entity *e = pool_get(&gs->entities, id);
    e->body.size = vec2(TUNING_DUMMY_W, TUNING_DUMMY_H);
    e->body.pos = vec2(feet.x - TUNING_DUMMY_W * 0.5f, feet.y - TUNING_DUMMY_H);
    e->prev_pos = e->body.pos;
    e->as.dummy = (Dummy){.hp = TUNING_DUMMY_HP, .max_hp = TUNING_DUMMY_HP, .team = team};
    return id;
}

static u32 count_live(const GameState *gs, EntityType type) {
    u32 n = 0;
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        n += (e->type == type && !e->pending_destroy) ? 1u : 0u;
    }
    return n;
}

u32 dummy_count(const GameState *gs) {
    return count_live(gs, ENT_DUMMY);
}

u32 projectile_count(const GameState *gs) {
    return count_live(gs, ENT_PROJECTILE);
}

// ---- The player's weapon -----------------------------------------------------------------------

void soldier_give_weapon(Companion *p, WeaponKind kind) {
    p->weapon = kind;
    p->ammo = weapon_def(kind)->max_ammo;
    p->fire_cooldown = 0;
}

void combat_give_weapon(GameState *gs, WeaponKind kind) {
    soldier_give_weapon(game_player(gs), kind);
}

static Vec2 drop_pos(const Companion *p) {
    const Vec2 c = body_center(&p->body);
    return vec2(c.x - TUNING_LOOT_W * 0.5f, p->body.pos.y + p->body.size.y - TUNING_LOOT_H);
}

static void equip(Companion *p, WeaponKind kind, i32 ammo) {
    p->weapon = kind;
    p->ammo = ammo;
    p->fire_cooldown = 0;
}

// Picks up a weapon the soldier stands on: automatically when unarmed, on the grab key (swapping
// with the weapon in hand) otherwise.
static void pickup_update(GameState *gs, Companion *p, bool grab) {
    const bool unarmed = p->weapon == WEAPON_FISTS;
    if (!unarmed && !grab) {
        return;
    }
    const Rect body = body_rect(&p->body);
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        Entity *e = &gs->entities.slots[i];
        if (e->type != ENT_WEAPON || e->pending_destroy || !rect_overlap(body, body_rect(&e->body))) {
            continue;
        }
        const WeaponItem item = e->as.weapon;
        if (!unarmed) {
            if (!entity_id_valid(loot_spawn_weapon(gs, p->weapon, p->ammo, drop_pos(p)))) {
                return; // no room to drop the old one: keep it
            }
            events_push(&gs->events, EV_WEAPON_DROPPED, body_center(&p->body), (i32)p->weapon, 0);
        }
        equip(p, item.kind, item.ammo);
        events_push(&gs->events, EV_WEAPON_PICKED_UP, body_center(&p->body), (i32)item.kind, 0);
        pool_mark_destroy(&gs->entities, slot_id(gs, i));
        return;
    }
}

static void punch(GameState *gs, Soldier *s, const WeaponDef *def) {
    Companion *p = &s->c;
    const Body *b = &p->body;
    const Rect fist = {p->facing > 0 ? b->pos.x + b->size.x : b->pos.x - def->reach, b->pos.y + 4.0f,
                       def->reach, b->size.y - 8.0f};
    Target targets[MAX_TARGETS];
    const u32 n = collect_targets(gs, targets);
    i32 hits = 0;
    for (u32 i = 0; i < n; i++) {
        if (targets[i].team != s->team && rect_overlap(fist, targets[i].rect)) {
            hurt(gs, &targets[i], def->damage, vec2((f32)p->facing * def->knockback, 0.0f));
            hits++;
        }
    }
    p->punch_ticks = PUNCH_ANIM_TICKS;
    p->fire_cooldown = def->cooldown_ticks;
    events_push(&gs->events, EV_PUNCH, rect_center(fist), hits, 0);
}

// Where the soldier's arm is: shots start from here.
static Vec2 shoulder(const Companion *p) {
    return vec2(p->body.pos.x + p->body.size.x * 0.5f, p->body.pos.y + TUNING_MUZZLE_HEIGHT);
}

// Points the weapon: at the mouse when aiming (the soldier also turns to face it), else along the facing.
static void update_aim(Companion *p, const InputFrame *in) {
    p->aim_dir = vec2((f32)p->facing, 0.0f);
    if (!in->aim_active) {
        return;
    }
    const Vec2 from = shoulder(p);
    const f32 dx = in->aim.x - from.x, dy = in->aim.y - from.y;
    const f32 len = sqrtf(dx * dx + dy * dy);
    if (fabsf(dx) > 1.0f) {
        p->facing = dx > 0.0f ? 1 : -1;
    }
    if (len >= TUNING_AIM_MIN_DIST) {
        p->aim_dir = vec2(dx / len, dy / len);
    } else {
        p->aim_dir = vec2((f32)p->facing, 0.0f);
    }
}

// A lob that lands on the aim point: flight time grows with the distance (within limits), the
// velocity follows from it. Without an aim it is the weapon's fixed throw in the facing direction.
static Vec2 grenade_velocity(const WeaponDef *def, const Companion *p, const InputFrame *in) {
    if (!in->aim_active) {
        return vec2((f32)p->facing * def->speed, -def->lift);
    }
    const Vec2 from = shoulder(p);
    const f32 dx = in->aim.x - from.x, dy = in->aim.y - from.y;
    const f32 dist = sqrtf(dx * dx + dy * dy);
    const f32 t = clampf(dist / (def->speed * 1.2f), 0.3f, 1.0f);
    return vec2(clampf(dx / t, -2.2f * def->speed, 2.2f * def->speed),
                clampf(dy / t - 0.5f * TUNING_GRAVITY * t, -2.2f * def->lift, 2.2f * def->lift));
}

static bool fire(GameState *gs, Soldier *s, const WeaponDef *def, const InputFrame *in) {
    Companion *p = &s->c;
    const Vec2 c = body_center(&p->body);
    // Muzzle a little way along the aim; if that spot is inside a wall, shoot from the shoulder instead.
    const Vec2 sh = shoulder(p);
    Vec2 muzzle = vec2_add(sh, vec2_scale(p->aim_dir, p->body.size.x * 0.5f + 3.0f));
    const Rect mr = {muzzle.x - def->proj_w * 0.5f, muzzle.y - def->proj_h * 0.5f, def->proj_w, def->proj_h};
    if (rect_hits_solid(gs->level, mr)) {
        muzzle = sh;
    }
    Vec2 vel;
    if (def->shot == SHOT_GRENADE) {
        vel = grenade_velocity(def, p, in);
    } else {
        vel = vec2_scale(p->aim_dir, def->speed);
        if (def->spread > 0.0f) { // random sideways speed: the burst fans out
            const f32 jitter = (rng_f32(&gs->rng) * 2.0f - 1.0f) * def->spread * def->speed;
            vel = vec2_add(vel, vec2(-p->aim_dir.y * jitter, p->aim_dir.x * jitter));
        }
    }
    if (!entity_id_valid(projectile_spawn(gs, p->weapon, s->team, muzzle, vel))) {
        return false;
    }
    gs->combat_stats.shots++;
    events_push(&gs->events, EV_SHOT, muzzle, (i32)p->weapon, p->facing);
    p->fire_cooldown = def->cooldown_ticks;
    if (def->max_ammo > 0 && --p->ammo <= 0) {
        events_push(&gs->events, EV_WEAPON_EMPTY, c, (i32)p->weapon, 0);
        p->weapon = WEAPON_FISTS;
        p->ammo = 0;
    }
    return true;
}

void combat_soldier_update(GameState *gs, Soldier *s, const InputFrame *in, bool crate_busy) {
    Companion *p = &s->c;
    if (p->fire_cooldown > 0) {
        p->fire_cooldown--;
    }
    if (p->hurt_ticks > 0) {
        p->hurt_ticks--;
    }
    if (p->punch_ticks > 0) {
        p->punch_ticks--;
    }
    if (p->hp <= 0) {
        return;
    }
    update_aim(p, in);
    if (crate_busy || entity_id_valid(p->carrying)) {
        return;
    }
    pickup_update(gs, p, input_pressed(in, ACT_GRAB));

    const WeaponDef *def = weapon_def(p->weapon);
    const bool trigger = def->automatic ? input_down(in, ACT_FIRE) : input_pressed(in, ACT_FIRE);
    if (!trigger || p->fire_cooldown > 0) {
        return;
    }
    if (def->shot == SHOT_MELEE) {
        punch(gs, s, def);
    } else {
        fire(gs, s, def, in);
    }
}

void combat_drop_loot(GameState *gs, Companion *p) {
    if (p->weapon != WEAPON_FISTS && p->ammo > 0 &&
        entity_id_valid(loot_spawn_weapon(gs, p->weapon, p->ammo, drop_pos(p)))) {
        events_push(&gs->events, EV_WEAPON_DROPPED, body_center(&p->body), (i32)p->weapon, 0);
    }
    crate_release_carried(gs, p);
}
