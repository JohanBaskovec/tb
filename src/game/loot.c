#include "game/loot.h"

#include "game/tuning.h"

bool weapon_from_product(PadProduct product, WeaponKind *out) {
    switch (product) {
    case PRODUCT_PISTOL:
        *out = WEAPON_PISTOL;
        return true;
    case PRODUCT_UZI:
        *out = WEAPON_UZI;
        return true;
    case PRODUCT_GRENADES:
        *out = WEAPON_GRENADES;
        return true;
    case PRODUCT_STRONG_GRENADES:
        *out = WEAPON_STRONG_GRENADES;
        return true;
    case PRODUCT_ROCKET_LAUNCHER:
        *out = WEAPON_ROCKET_LAUNCHER;
        return true;
    default:
        return false;
    }
}

EntityId loot_spawn_weapon(GameState *gs, WeaponKind kind, i32 ammo, Vec2 pos) {
    EntityId id = pool_create(&gs->entities, ENT_WEAPON);
    if (!entity_id_valid(id)) {
        return ENTITY_NONE;
    }
    Entity *e = pool_get(&gs->entities, id);
    e->body.size = vec2(TUNING_LOOT_W, TUNING_LOOT_H);
    e->body.pos = pos;
    e->prev_pos = pos;
    e->as.weapon = (WeaponItem){.kind = kind, .ammo = ammo};
    return id;
}

void loot_update(GameState *gs) {
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        Entity *e = &gs->entities.slots[i];
        if ((e->type != ENT_PRODUCT && e->type != ENT_WEAPON) || e->pending_destroy) {
            continue;
        }
        e->prev_pos = e->body.pos;
        for (int lift = 0; lift < 4; lift++) {
            const Vec2 c = body_center(&e->body);
            if (!level_is_solid(gs->level, level_px_to_tile(c.x), level_px_to_tile(c.y))) {
                break;
            }
            e->body.pos.y -= (f32)TILE_SIZE;
            e->prev_pos = e->body.pos;
            e->body.vel.y = 0.0f;
        }
        e->body.vel.y = MIN(e->body.vel.y + TUNING_GRAVITY * TICK_DT, TUNING_MAX_FALL_SPEED);
        body_move(&e->body, gs->level, TICK_DT, false);
        if (e->body.on_ground) {
            e->body.vel.y = 0.0f;
        }
        if (body_overlaps_tile(&e->body, gs->level, TILE_HAZARD)) {
            pool_mark_destroy(&gs->entities, (EntityId){i, e->generation});
        }
    }
}
