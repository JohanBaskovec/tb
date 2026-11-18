#include "app/fx.h"

#include "game/weapon.h"

#include <math.h>
#include <string.h>

#define DT (1.0f / (f32)TICK_HZ)
#define MAX_SHAKE 7.0f
#define LAND_SQUASH_TICKS 6

void fx_init(FxList *fx) {
    memset(fx, 0, sizeof(*fx));
    rng_seed(&fx->rng, 0xF00D, 7);
}

// ---- Short effects -----------------------------------------------------------------------------

static void add(FxList *l, FxKind kind, Vec2 pos, f32 radius, u32 life) {
    if (l->count >= MAX_FX) {
        return; // effects are only decoration: drop the new one
    }
    l->items[l->count++] = (Fx){kind, pos, radius, 0, life};
}

// ---- Particles ---------------------------------------------------------------------------------

typedef struct Rgb {
    u8 r, g, b;
} Rgb;

static const Rgb DIRT[3] = {{104, 78, 62}, {74, 66, 76}, {142, 106, 72}};
static const Rgb STONE[2] = {{96, 92, 110}, {60, 56, 72}};
static const Rgb WOOD[2] = {{176, 124, 66}, {112, 74, 38}};
static const Rgb SPARK = {255, 224, 120};
static const Rgb DUST = {206, 186, 156};
static const Rgb WATER = {120, 180, 240};
static const Rgb BRASS = {224, 184, 76};
static const Rgb STAR_TINT = {255, 255, 255};

static f32 rnd(FxList *fx, f32 lo, f32 hi) {
    return lo + (hi - lo) * rng_f32(&fx->rng);
}

static Particle *spawn(FxList *fx, ParticleKind kind, Vec2 pos, Vec2 vel, Rgb c, u32 life) {
    if (fx->part_count >= MAX_PARTICLES) {
        return NULL;
    }
    Particle *p = &fx->parts[fx->part_count++];
    *p = (Particle){.kind = (u8)kind, .r = c.r, .g = c.g, .b = c.b, .pos = pos, .vel = vel, .drag = 1.0f, .size = 2.0f,
                    .life = (u16)MAX(life, 1u)};
    return p;
}

// Speed `lo..hi` in a direction between `a0` and `a1` degrees (0 = right, -90 = up).
static Vec2 fling(FxList *fx, f32 a0, f32 a1, f32 lo, f32 hi) {
    const f32 a = rnd(fx, a0, a1) * 3.14159265f / 180.0f, s = rnd(fx, lo, hi);
    return vec2(cosf(a) * s, sinf(a) * s);
}

static void debris(FxList *fx, Vec2 pos, u32 n, f32 speed, const Rgb *colors, u32 ncolors) {
    for (u32 i = 0; i < n; i++) {
        Particle *p = spawn(fx, PART_CHUNK, pos, fling(fx, -165.0f, -15.0f, speed * 0.4f, speed),
                            colors[rng_below(&fx->rng, ncolors)], (u32)rnd(fx, 45.0f, 80.0f));
        if (p) {
            p->gravity = 650.0f;
            p->size = rnd(fx, 1.5f, 3.2f);
            p->spin = rnd(fx, -720.0f, 720.0f);
            p->collide = true;
        }
    }
}

static void smoke(FxList *fx, Vec2 pos, u32 n, f32 spread, f32 size, u8 grey) {
    for (u32 i = 0; i < n; i++) {
        const Vec2 at = vec2(pos.x + rnd(fx, -spread, spread), pos.y + rnd(fx, -spread, spread) * 0.6f);
        const u8 g = (u8)(grey + (u8)rnd(fx, 0.0f, 24.0f));
        Particle *p = spawn(fx, PART_SMOKE, at, vec2(rnd(fx, -18.0f, 18.0f), rnd(fx, -36.0f, -10.0f)), (Rgb){g, g, g},
                            (u32)rnd(fx, 40.0f, 75.0f));
        if (p) {
            p->size = size * rnd(fx, 0.7f, 1.2f);
            p->grow = size * 0.6f;
            p->drag = 0.4f;
        }
    }
}

static void sparks(FxList *fx, Vec2 pos, u32 n, f32 speed) {
    for (u32 i = 0; i < n; i++) {
        Particle *p = spawn(fx, PART_SPARK, pos, fling(fx, -180.0f, 180.0f, speed * 0.4f, speed), SPARK,
                            (u32)rnd(fx, 8.0f, 18.0f));
        if (p) {
            p->gravity = 300.0f;
            p->drag = 0.2f;
        }
    }
}

// Little puffs at the feet, pushed sideways.
static void dust(FxList *fx, Vec2 feet, u32 n, f32 push) {
    for (u32 i = 0; i < n; i++) {
        const f32 side = (i % 2 == 0) ? 1.0f : -1.0f;
        Particle *p = spawn(fx, PART_SMOKE, vec2(feet.x + side * rnd(fx, 1.0f, 5.0f), feet.y - 1.0f),
                            vec2(side * rnd(fx, push * 0.4f, push), rnd(fx, -22.0f, -6.0f)), DUST, (u32)rnd(fx, 16.0f, 28.0f));
        if (p) {
            p->size = rnd(fx, 1.5f, 2.6f);
            p->grow = 5.0f;
            p->drag = 0.05f;
        }
    }
}

static void stars(FxList *fx, Vec2 pos, u32 n, f32 rise) {
    for (u32 i = 0; i < n; i++) {
        Particle *p = spawn(fx, PART_STAR, vec2(pos.x + rnd(fx, -10.0f, 10.0f), pos.y + rnd(fx, -6.0f, 6.0f)),
                            vec2(rnd(fx, -25.0f, 25.0f), -rnd(fx, rise * 0.5f, rise)), STAR_TINT, (u32)rnd(fx, 24.0f, 40.0f));
        if (p) {
            p->drag = 0.3f;
            p->spin = rnd(fx, -200.0f, 200.0f);
        }
    }
}

static void ring(FxList *fx, Vec2 pos, f32 radius, Rgb c, u32 life) {
    Particle *p = spawn(fx, PART_RING, pos, vec2(0.0f, 0.0f), c, life);
    if (p) {
        p->size = radius * 0.3f;
        p->grow = radius * 0.7f / ((f32)life * DT);
    }
}

static void droplets(FxList *fx, Vec2 pos, u32 n) {
    for (u32 i = 0; i < n; i++) {
        Particle *p = spawn(fx, PART_DOT, pos, fling(fx, -130.0f, -50.0f, 60.0f, 170.0f), WATER, (u32)rnd(fx, 25.0f, 45.0f));
        if (p) {
            p->gravity = 600.0f;
            p->size = rnd(fx, 1.0f, 2.5f);
        }
    }
}

static void shell(FxList *fx, Vec2 muzzle, i32 facing) {
    const f32 back = facing < 0 ? 1.0f : -1.0f;
    Particle *p = spawn(fx, PART_CHUNK, vec2(muzzle.x + back * 6.0f, muzzle.y),
                        vec2(back * rnd(fx, 30.0f, 70.0f), -rnd(fx, 70.0f, 130.0f)), BRASS, 70);
    if (p) {
        p->gravity = 650.0f;
        p->size = 1.5f;
        p->spin = rnd(fx, -900.0f, 900.0f);
        p->collide = true;
    }
}

static void corpse(FxList *fx, Vec2 center, i32 team, const GameState *gs, i32 index) {
    i32 variant = 0, facing = 1;
    if (gs && index >= 0 && index < MAX_SOLDIERS) {
        variant = (i32)gs->soldiers[index].variant;
        facing = gs->soldiers[index].c.facing;
    }
    Particle *p = spawn(fx, PART_CORPSE, center, vec2(-(f32)facing * rnd(fx, 50.0f, 90.0f), -rnd(fx, 170.0f, 220.0f)),
                        STAR_TINT, 80);
    if (p) {
        p->team = (u8)MIN(MAX(team, 0), MAX_TEAMS - 1);
        p->variant = (u8)MIN(MAX(variant, 0), SPRITE_VARIANTS - 1);
        p->facing = (i8)(facing < 0 ? -1 : 1);
        p->gravity = 700.0f;
        p->spin = -(f32)facing * rnd(fx, 420.0f, 640.0f);
    }
}

// Shake from a blast, weaker away from where the player looks.
static void shake(FxList *fx, Vec2 pos, f32 amount) {
    const f32 dx = pos.x - fx->focus.x, dy = pos.y - fx->focus.y;
    const f32 near = clampf(1.0f - (sqrtf(dx * dx + dy * dy) - 160.0f) / 480.0f, 0.0f, 1.0f);
    fx->shake = MIN(MAX_SHAKE, fx->shake + amount * near);
}

static bool in_water(const GameState *gs, Vec2 p) {
    return gs && gs->level && level_tile_at_px(gs->level, p.x, p.y) == TILE_HAZARD;
}

static void explosion(FxList *fx, Vec2 pos, f32 radius) {
    const f32 k = radius / 40.0f;
    add(fx, FX_EXPLOSION, pos, radius, 20);
    ring(fx, pos, radius * 1.3f, (Rgb){255, 240, 200}, 12);
    debris(fx, pos, 8u + (u32)(radius / 5.0f), 240.0f * k, DIRT, 3);
    smoke(fx, pos, 5u + (u32)(radius / 8.0f), radius * 0.4f, radius * 0.22f, 92);
    sparks(fx, pos, 10, 340.0f * k);
    shake(fx, pos, radius / 9.0f);
}

void fx_consume(FxList *fx, const EventQueue *events, const GameState *gs) {
    for (u32 i = 0; i < events->count; i++) {
        const GameEvent *e = &events->items[i];
        switch (e->type) {
        case EV_EXPLOSION:
            explosion(fx, e->pos, (f32)e->a);
            break;
        case EV_DUMMY_DESTROYED:
            add(fx, FX_EXPLOSION, e->pos, 14.0f, 14);
            debris(fx, e->pos, 10, 200.0f, WOOD, 2);
            break;
        case EV_SOLDIER_DIED:
            add(fx, FX_EXPLOSION, e->pos, 12.0f, 12);
            corpse(fx, e->pos, e->a, gs, e->b);
            smoke(fx, e->pos, 6, 6.0f, 4.0f, 180);
            stars(fx, e->pos, 3, 40.0f);
            break;
        case EV_PROJECTILE_HIT:
            add(fx, FX_SPARK, e->pos, 5.0f, 8);
            if (e->b == 1) { // into a wall
                sparks(fx, e->pos, 4, 160.0f);
                dust(fx, e->pos, 2, 20.0f);
            }
            break;
        case EV_DAMAGE:
            add(fx, FX_SPARK, e->pos, 5.0f, 8);
            if (e->b == 2) {
                debris(fx, e->pos, 4, 150.0f, STONE, 2);
            } else if (e->b == 0) {
                debris(fx, e->pos, 3, 120.0f, WOOD, 2);
            } else {
                sparks(fx, e->pos, 5, 180.0f);
            }
            break;
        case EV_SHOT:
            add(fx, FX_MUZZLE, e->pos, 4.0f, 3);
            if (e->a == WEAPON_PISTOL || e->a == WEAPON_UZI) {
                shell(fx, e->pos, e->b);
            } else if (e->a == WEAPON_ROCKET_LAUNCHER) {
                smoke(fx, vec2(e->pos.x - (f32)e->b * 10.0f, e->pos.y), 4, 3.0f, 3.0f, 170);
            }
            break;
        case EV_BASE_DESTROYED:
            explosion(fx, e->pos, 40.0f);
            debris(fx, e->pos, 28, 260.0f, STONE, 2);
            smoke(fx, e->pos, 12, 18.0f, 8.0f, 70);
            shake(fx, e->pos, 6.0f);
            break;
        case EV_SOLDIER_SPAWNED:
            ring(fx, vec2(e->pos.x, e->pos.y), 14.0f, (Rgb){255, 255, 255}, 10);
            smoke(fx, e->pos, 5, 6.0f, 3.5f, 200);
            stars(fx, e->pos, 3, 50.0f);
            break;
        case EV_PAD_BUILT:
            stars(fx, e->pos, 8, 70.0f);
            smoke(fx, e->pos, 4, 14.0f, 4.0f, 210);
            break;
        case EV_CRATE_DESTROYED: {
            const Vec2 c = vec2(e->pos.x + 8.0f, e->pos.y + 8.0f);
            if (in_water(gs, c) || in_water(gs, vec2(c.x, c.y + 8.0f))) {
                droplets(fx, c, 12);
            } else {
                debris(fx, c, 8, 170.0f, WOOD, 2);
            }
            break;
        }
        case EV_CRATE_CONSUMED:
            debris(fx, vec2(e->pos.x + 8.0f, e->pos.y + 8.0f), 4, 120.0f, WOOD, 2);
            break;
        case EV_CRATE_SETTLED:
            dust(fx, vec2(e->pos.x + 8.0f, e->pos.y + 16.0f), 4, 30.0f);
            break;
        case EV_JUMP:
            dust(fx, e->pos, 3, 18.0f);
            break;
        case EV_LANDED:
            dust(fx, e->pos, 5, 34.0f);
            if (e->b >= 0 && e->b < MAX_SOLDIERS) {
                fx->anim[e->b].land_ticks = LAND_SQUASH_TICKS;
            }
            break;
        case EV_WEAPON_PICKED_UP:
            stars(fx, e->pos, 3, 40.0f);
            break;
        default:
            break;
        }
    }
}

// ---- Per tick ----------------------------------------------------------------------------------

static void observe_soldiers(FxList *fx, const GameState *gs) {
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        const Soldier *s = &gs->soldiers[i];
        SoldierAnim *a = &fx->anim[i];
        if (!s->alive) {
            a->alive = false;
            continue;
        }
        if (!a->alive) {
            *a = (SoldierAnim){.alive = true, .on_ground = s->c.body.on_ground};
        }
        const Companion *c = &s->c;
        if (c->body.on_ground) {
            a->stride += fabsf(c->body.pos.x - c->prev_pos.x);
        }
        if (c->on_ladder) {
            a->climb += fabsf(c->body.pos.y - c->prev_pos.y);
        }
        if (a->land_ticks > 0) {
            a->land_ticks--;
        }
        a->on_ground = c->body.on_ground;
    }
}

static void rocket_trails(FxList *fx, const GameState *gs) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type != ENT_PROJECTILE || e->pending_destroy || weapon_def(e->as.projectile.weapon)->shot != SHOT_ROCKET) {
            continue;
        }
        const Vec2 v = e->body.vel;
        const f32 sp = sqrtf(v.x * v.x + v.y * v.y);
        const Vec2 c = vec2(e->body.pos.x + e->body.size.x * 0.5f, e->body.pos.y + e->body.size.y * 0.5f);
        const Vec2 tail = sp > 0.0f ? vec2(c.x - v.x / sp * 7.0f, c.y - v.y / sp * 7.0f) : c;
        Particle *p = spawn(fx, PART_SMOKE, tail, vec2(rnd(fx, -8.0f, 8.0f), rnd(fx, -14.0f, -4.0f)), (Rgb){214, 214, 214},
                            (u32)rnd(fx, 20.0f, 32.0f));
        if (p) {
            p->size = 1.5f;
            p->grow = 9.0f;
            p->drag = 0.3f;
        }
        if (gs->tick % 3 == 0) {
            sparks(fx, tail, 1, 60.0f);
        }
    }
}

void fx_observe(FxList *fx, const GameState *gs) {
    fx->tick = gs->tick;
    observe_soldiers(fx, gs);
    rocket_trails(fx, gs);
}

static void step_particle(Particle *p, const Level *lv) {
    p->vel.y += p->gravity * DT;
    if (p->drag < 1.0f) {
        const f32 keep = powf(MAX(p->drag, 0.001f), DT);
        p->vel = vec2_scale(p->vel, keep);
    }
    Vec2 next = vec2_add(p->pos, vec2_scale(p->vel, DT));
    if (p->collide && lv) {
        if (level_is_solid(lv, level_px_to_tile(next.x), level_px_to_tile(p->pos.y))) {
            p->vel.x *= -0.4f;
            next.x = p->pos.x;
        }
        const i32 tx = level_px_to_tile(next.x), ty = level_px_to_tile(next.y);
        const bool falling_onto_platform =
            p->vel.y > 0.0f && level_is_one_way(lv, tx, ty) && level_px_to_tile(p->pos.y) < ty;
        if (level_is_solid(lv, tx, ty) || falling_onto_platform) {
            next.y = p->pos.y;
            p->vel.y = fabsf(p->vel.y) < 40.0f ? 0.0f : -p->vel.y * 0.35f;
            p->vel.x *= 0.6f;
            p->spin *= 0.5f;
        }
    }
    p->pos = next;
    p->size = MAX(0.0f, p->size + p->grow * DT);
    p->angle += p->spin * DT;
}

void fx_step(FxList *fx, const Level *lv) {
    u32 kept = 0;
    for (u32 i = 0; i < fx->count; i++) {
        Fx f = fx->items[i];
        f.age++;
        if (f.age < f.life) {
            fx->items[kept++] = f;
        }
    }
    fx->count = kept;
    kept = 0;
    for (u32 i = 0; i < fx->part_count; i++) {
        Particle p = fx->parts[i];
        if (++p.age >= p.life) {
            continue;
        }
        step_particle(&p, lv);
        fx->parts[kept++] = p;
    }
    fx->part_count = kept;
    fx->shake = fx->shake < 0.2f ? 0.0f : fx->shake * 0.84f;
}

// ---- Queries -----------------------------------------------------------------------------------

BodyPose fx_pose(const FxList *fx, const GameState *gs, i32 soldier) {
    const Companion *c = &gs->soldiers[soldier].c;
    const SoldierAnim *a = &fx->anim[soldier];
    if (c->on_ladder) {
        return ((i32)(a->climb / 7.0f) % 2) ? POSE_CLIMB1 : POSE_CLIMB0;
    }
    if (!c->body.on_ground) {
        return c->body.vel.y < -40.0f ? POSE_JUMP : POSE_FALL;
    }
    if (fabsf(c->body.vel.x) > 12.0f) {
        return (BodyPose)(POSE_RUN0 + (i32)(a->stride / 7.0f) % 4);
    }
    return ((fx->tick + (u64)soldier * 13u) / 36u) % 2 ? POSE_IDLE1 : POSE_IDLE0;
}

f32 fx_squash(const FxList *fx, i32 soldier) {
    return (f32)fx->anim[soldier].land_ticks / (f32)LAND_SQUASH_TICKS;
}

Vec2 fx_shake_offset(const FxList *fx) {
    if (fx->shake <= 0.0f) {
        return vec2(0.0f, 0.0f);
    }
    const f32 t = (f32)fx->tick;
    return vec2(fx->shake * sinf(t * 2.7f + 1.0f), fx->shake * cosf(t * 3.3f));
}
