#include "app/fx.h"

static void add(FxList *l, FxKind kind, Vec2 pos, f32 radius, u32 life) {
    if (l->count >= MAX_FX) {
        return; // effects are only decoration: drop the new one
    }
    l->items[l->count++] = (Fx){kind, pos, radius, 0, life};
}

void fx_consume(FxList *list, const EventQueue *events) {
    for (u32 i = 0; i < events->count; i++) {
        const GameEvent *e = &events->items[i];
        switch (e->type) {
        case EV_EXPLOSION:
            add(list, FX_EXPLOSION, e->pos, (f32)e->a, 20);
            break;
        case EV_DUMMY_DESTROYED:
            add(list, FX_EXPLOSION, e->pos, 14.0f, 14);
            break;
        case EV_PLAYER_DIED:
            add(list, FX_EXPLOSION, e->pos, 18.0f, 16);
            break;
        case EV_PROJECTILE_HIT:
        case EV_DAMAGE:
            add(list, FX_SPARK, e->pos, 5.0f, 8);
            break;
        case EV_SHOT:
            add(list, FX_MUZZLE, e->pos, 4.0f, 3);
            break;
        default:
            break;
        }
    }
}

void fx_step(FxList *list) {
    u32 kept = 0;
    for (u32 i = 0; i < list->count; i++) {
        Fx fx = list->items[i];
        fx.age++;
        if (fx.age < fx.life) {
            list->items[kept++] = fx;
        }
    }
    list->count = kept;
}
