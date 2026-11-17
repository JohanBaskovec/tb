// Entity pool with generational ids. Fixed capacity, no allocation, deterministic reuse order.
//
// An EntityId stays valid only as long as its entity lives: once the slot is freed (and possibly
// reused) the old id resolves to NULL. Never keep a raw Entity pointer across ticks, keep the id.
// Destruction is deferred: pool_mark_destroy flags the entity, pool_flush (end of tick) frees it.
// Companions join the pool with the squad milestone; for now crates, pad products, weapon pickups,
// projectiles and target dummies live here.
#pragma once

#include "core/config.h"
#include "game/physics.h"
#include "game/weapon.h"

typedef struct EntityId {
    u16 index;      // 0 = none; slots are 1..MAX_ENTITIES
    u16 generation; // 0 = none
} EntityId;

#define ENTITY_NONE ((EntityId){0, 0})

static inline bool entity_id_valid(EntityId id) {
    return id.index != 0;
}

static inline bool entity_id_eq(EntityId a, EntityId b) {
    return a.index == b.index && a.generation == b.generation;
}

typedef enum EntityType {
    ENT_NONE = 0, // free slot
    ENT_CRATE,
    ENT_PRODUCT, // result of a pad construction that is not a weapon yet (soldiers, vehicles: placeholder)
    ENT_WEAPON,     // a weapon lying on the ground, to be picked up
    ENT_PROJECTILE, // bullet, grenade or rocket in flight
    ENT_DUMMY,      // immobile target with hit points
    ENT_TYPE_COUNT
} EntityType;

typedef enum PadProduct {
    PRODUCT_PISTOL,
    PRODUCT_UZI,
    PRODUCT_GRENADES,
    PRODUCT_STRONG_GRENADES,
    PRODUCT_COMPANION,
    PRODUCT_STRONG_COMPANION,
    PRODUCT_ROCKET_LAUNCHER,
    PRODUCT_HEAVY_COMPANION,
    PRODUCT_HEAVY_ARMORED_COMPANION,
    PRODUCT_TANK,
    PRODUCT_HEAVY_TANK,
    PRODUCT_COUNT
} PadProduct;

typedef enum CrateKind {
    CRATE_RED,
    CRATE_BLUE, // stronger: one blue crate in a pad shape gives the upgraded result
} CrateKind;

typedef enum CrateState {
    CRATE_DESCENDING, // slowly coming down from the sky, constant speed
    CRATE_FALLING,    // free body under gravity (dropped, thrown, or lost its support)
    CRATE_RESTING,    // locked in a grid cell, solid
    CRATE_CARRIED,    // held above a companion's head
} CrateState;

typedef struct Crate {
    CrateKind kind;
    CrateState state;
    i32 cell_x, cell_y; // grid cell, valid while RESTING
    i32 origin_zone;    // drop zone that produced it, -1 if none
} Crate;

typedef struct Product {
    PadProduct kind;
    i32 team; // team whose pad made it
} Product;

typedef struct WeaponItem {
    WeaponKind kind;
    i32 ammo;
} WeaponItem;

typedef struct Projectile {
    WeaponKind weapon; // which weapon fired it (its definition says how it flies and explodes)
    i32 team;          // shooter's team: bullets do not hurt it
    u32 ttl;           // ticks left: range of a bullet, fuse of a grenade
} Projectile;

typedef struct Dummy {
    i32 hp, max_hp;
    i32 team;
    u32 hurt_ticks; // > 0: flashing after a hit (presentation, but part of the state)
} Dummy;

typedef struct Entity {
    EntityType type;
    u16 generation;
    bool pending_destroy;
    Body body; // position (top-left), velocity, size
    Vec2 prev_pos; // position at the start of the last tick, for render interpolation
    union {
        Crate crate;
        Product product;
        WeaponItem weapon;
        Projectile projectile;
        Dummy dummy;
    } as;
} Entity;

typedef struct EntityPool {
    Entity slots[MAX_ENTITIES + 1]; // slot 0 is never used
    u16 free_list[MAX_ENTITIES];
    u16 free_count;
    u16 live_count;
} EntityPool;

void pool_init(EntityPool *p);
// Returns ENTITY_NONE when the pool is full. The entity is zeroed apart from type and generation.
EntityId pool_create(EntityPool *p, EntityType type);
// NULL for invalid, stale or freed ids.
Entity *pool_get(EntityPool *p, EntityId id);
const Entity *pool_get_const(const EntityPool *p, EntityId id);
void pool_mark_destroy(EntityPool *p, EntityId id);
// Frees every entity flagged with pool_mark_destroy.
void pool_flush(EntityPool *p);
