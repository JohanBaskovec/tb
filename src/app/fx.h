// Everything visual the game itself does not track: short effects (explosions, sparks, muzzle flashes), particles
// (debris, smoke, dust, splinters, droplets, sparkles, shell casings, a soldier knocked out of the match), the animation
// state of each soldier (stride, landing squash) and the screen shake. Built from the game's events and state.
//
// Pure presentation, no SDL: the game never reads it, so it cannot change gameplay or the hash. Its random numbers come
// from its own generator, never from the game's.
#pragma once

#include "core/rng.h"
#include "game/game.h"
#include "render/sprites.h"

#define MAX_FX 64
#define MAX_PARTICLES 640

typedef enum FxKind {
    FX_EXPLOSION, // radius = blast radius
    FX_SPARK,     // something was hit
    FX_MUZZLE,    // a shot left the weapon
} FxKind;

typedef struct Fx {
    FxKind kind;
    Vec2 pos;
    f32 radius;
    u32 age, life; // ticks
} Fx;

typedef enum ParticleKind {
    PART_DOT,    // a square of `size` px
    PART_SMOKE,  // a soft disc that grows and fades
    PART_SPARK,  // a bright streak along its velocity
    PART_CHUNK,  // a tumbling bit of debris
    PART_STAR,   // the sparkle sprite
    PART_RING,   // a shock wave: a circle that grows
    PART_CORPSE, // a soldier sprite tumbling away
    PART_ICON,   // a sprite (`variant` is its SpriteId) floating up and fading: hearts, ammunition
} ParticleKind;

typedef struct Particle {
    u8 kind;
    u8 r, g, b;
    u8 team, variant; // PART_CORPSE; variant: PART_ICON sprite
    i8 facing;
    bool collide; // stops and bounces on the terrain
    Vec2 pos, vel;
    f32 gravity, drag; // px/s², fraction of speed kept per second
    f32 size, grow;    // px, px per second
    f32 angle, spin;   // degrees, degrees per second
    u16 age, life;     // ticks
} Particle;

typedef struct SoldierAnim {
    bool alive;       // as seen last tick (a new soldier in the slot resets the rest)
    bool on_ground;
    f32 stride;       // px walked on the ground, drives the run cycle
    f32 climb;        // px climbed
    u32 land_ticks;   // > 0: squashed after a landing
} SoldierAnim;

typedef struct FxList {
    Fx items[MAX_FX];
    u32 count;
    Particle parts[MAX_PARTICLES];
    u32 part_count;
    SoldierAnim anim[MAX_SOLDIERS];
    u8 crate_prev[MAX_ENTITIES + 1]; // state a crate had last tick, plus one (0: not a crate), by pool slot
    u8 crate_land[MAX_ENTITIES + 1]; // ticks left of the squash after a crate landed
    f32 shake; // px of screen shake, decays every tick
    Vec2 focus; // world point the player looks at (camera center): far blasts shake less
    u64 tick;
    Rng rng;
} FxList;

void fx_init(FxList *fx);
// Turns the events of one tick into effects and particles. `gs` (may be NULL) gives the context: the terrain under
// an event, the soldier who died. Call once per game tick.
void fx_consume(FxList *fx, const EventQueue *events, const GameState *gs);
// Once per tick, after the game update: soldier animation, rocket smoke trails.
void fx_observe(FxList *fx, const GameState *gs);
// Ages effects and particles by one tick (particles bounce on `lv`'s terrain if it is not NULL).
void fx_step(FxList *fx, const Level *lv);

// Pose of a soldier for this frame.
BodyPose fx_pose(const FxList *fx, const GameState *gs, i32 soldier);
// 0 normally, up to 1 just after a landing (the body is drawn wider and shorter).
f32 fx_squash(const FxList *fx, i32 soldier);
// 0 normally, up to 1 just after the crate in pool slot `slot` landed or was put down (drawn wider and shorter).
f32 fx_crate_squash(const FxList *fx, u16 slot);
// Screen offset of the shake for this tick, in pixels.
Vec2 fx_shake_offset(const FxList *fx);

// Progress of an effect in [0, 1).
static inline f32 fx_progress(const Fx *fx) {
    return (f32)fx->age / (f32)fx->life;
}
static inline f32 particle_progress(const Particle *p) {
    return (f32)p->age / (f32)p->life;
}
