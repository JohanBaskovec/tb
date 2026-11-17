// Helpers shared by tests: build small levels in code, and drive a Companion with key masks.
#pragma once

#include "core/arena.h"
#include "core/input_frame.h"
#include "game/companion.h"
#include "game/level.h"

#include <stdio.h>
#include <string.h>

#define KEY(a) action_bit(a)

// What a pad built, as a product kind: a weapon lies on the ground as a weapon pickup, the rest as a
// generic product. Returns false for any other entity.
static inline bool entity_as_product(const Entity *e, PadProduct *out) {
    if (e->type == ENT_PRODUCT) {
        *out = e->as.product.kind;
        return true;
    }
    if (e->type != ENT_WEAPON) {
        return false;
    }
    switch (e->as.weapon.kind) {
    case WEAPON_PISTOL:
        *out = PRODUCT_PISTOL;
        return true;
    case WEAPON_UZI:
        *out = PRODUCT_UZI;
        return true;
    case WEAPON_GRENADES:
        *out = PRODUCT_GRENADES;
        return true;
    case WEAPON_STRONG_GRENADES:
        *out = PRODUCT_STRONG_GRENADES;
        return true;
    case WEAPON_ROCKET_LAUNCHER:
        *out = PRODUCT_ROCKET_LAUNCHER;
        return true;
    default:
        return false;
    }
}

typedef struct TestMap {
    int w, h;
    char c[MAX_LEVEL_W * MAX_LEVEL_H];
} TestMap;

static inline void map_init(TestMap *m, int w, int h) {
    m->w = w;
    m->h = h;
    memset(m->c, '.', sizeof(m->c));
}

static inline void map_put(TestMap *m, int x, int y, char ch) {
    m->c[y * m->w + x] = ch;
}

// Inclusive rectangle.
static inline void map_rect(TestMap *m, int x0, int y0, int x1, int y1, char ch) {
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            map_put(m, x, y, ch);
        }
    }
}

// Renders the map as level text (header-less).
static inline void map_text(const TestMap *m, char *out, usize cap) {
    usize n = (usize)snprintf(out, cap, "---\n");
    for (int y = 0; y < m->h; y++) {
        for (int x = 0; x < m->w; x++) {
            out[n++] = m->c[y * m->w + x];
        }
        out[n++] = '\n';
    }
    out[n] = '\0';
}

static inline bool map_load(const TestMap *m, Level *lv, Arena *arena) {
    static char text[MAX_LEVEL_W * MAX_LEVEL_H + MAX_LEVEL_H + 16];
    map_text(m, text, sizeof(text));
    char err[128];
    bool ok = level_parse(lv, arena, text, err, sizeof(err));
    if (!ok) {
        fprintf(stderr, "map_load failed: %s\n", err);
    }
    return ok;
}

// Feeds `mask` as the held keys for `ticks` ticks.
static inline void run_ticks(Companion *c, const Level *lv, InputFrame *in, u32 mask, int ticks) {
    for (int i = 0; i < ticks; i++) {
        input_frame_update(in, mask);
        companion_update(c, lv, in);
    }
}
