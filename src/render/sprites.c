#include "render/sprites.h"

#include <stdio.h>
#include <string.h>

// ---- Palette -----------------------------------------------------------------------------------
//
//  .  transparent        o  outline
//  T  team               t  team, dark            L  team, light
//  S  skin               s  skin, shaded          W  white          K  black (pupil)
//  B  boots              G  belt, straps          Y  yellow (buckle, sparkle)
//  M  metal              m  metal, dark           V  olive          v  olive, dark
//  w  wood               d  wood, dark            R  red (rocket head)
//  C  red crate panel    c  red crate, dark       U  blue crate     u  blue crate, dark

typedef struct PaletteEntry {
    char key;
    u8 r, g, b;
} PaletteEntry;

static const PaletteEntry FIXED_COLORS[] = {
    {'o', 34, 28, 42},    {'S', 244, 196, 150}, {'s', 206, 150, 112}, {'W', 250, 250, 250}, {'K', 24, 24, 32},
    {'B', 70, 58, 56},    {'G', 126, 96, 58},   {'Y', 255, 214, 80},  {'M', 176, 182, 196}, {'m', 112, 118, 134},
    {'V', 110, 134, 70},  {'v', 74, 92, 46},    {'w', 176, 124, 66},  {'d', 112, 74, 38},   {'R', 220, 50, 40},
    {'C', 210, 62, 52},   {'c', 140, 36, 32},   {'U', 60, 112, 228},  {'u', 34, 66, 150},
};

static bool known_key(char k) {
    if (k == '.' || k == 'T' || k == 't' || k == 'L') {
        return true;
    }
    for (usize i = 0; i < sizeof(FIXED_COLORS) / sizeof(FIXED_COLORS[0]); i++) {
        if (FIXED_COLORS[i].key == k) {
            return true;
        }
    }
    return false;
}

// ---- Parts -------------------------------------------------------------------------------------

typedef struct Part {
    const char *name;
    i32 w, h;
    const char *const *rows;
} Part;

#define PART(name, w, rows) {#name, w, (i32)(sizeof(rows) / sizeof(rows[0])), rows}

static const char *const HEAD[] = {
    ".....oooooo...",
    "....oLLTTTTo..",
    "...oLTTTTTTTo.",
    "...oTTTTTTTTo.",
    "..otttttttttto",
    "...oSSSSSWKSo.",
    "...oSSSSSWKSo.",
    "...osSSSSSSSo.",
    "....osSSSSso..",
    ".....oooooo...",
};

static const char *const HEAD_STRIPED[] = { // the strong soldier: a white stripe on the helmet
    ".....oooooo...",
    "....oLLWWTTo..",
    "...oLTTWWTTTo.",
    "...oTTTWWTTTo.",
    "..otttttttttto",
    "...oSSSSSWKSo.",
    "...oSSSSSWKSo.",
    "...osSSSSSSSo.",
    "....osSSSSso..",
    ".....oooooo...",
};

static const char *const TORSO[] = {
    "..otttttto..",
    ".oTTTTTTTTo.",
    ".oTLTTTTTTo.",
    ".oTLTTTTTTo.",
    ".otTTTTTTto.",
    ".oGGGGGYGGo.",
    ".otTTTTTTto.",
};

static const char *const TORSO_STRONG[] = { // chevrons on the chest
    "..otttttto..",
    ".oTTTTTTTTo.",
    ".oTLTTTTWWo.",
    ".oTLTTTWWTo.",
    ".otTTTTTTto.",
    ".oGGGGGYGGo.",
    ".otTTTTTTto.",
};

static const char *const TORSO_HEAVY[] = { // shoulder pads and straps
    "..oooooooooo..",
    ".oLLTTTTTTLLo.",
    "oLLoTTTTTToLLo",
    ".oooTGTTGTooo.",
    "...oTGTTGTo...",
    "...otTTTTto...",
    "...oGGGYGGo...",
    "...otTTTTto...",
};

static const char *const LEGS_IDLE[] = {
    "...otto.otto..",
    "...otto.otto..",
    "...otto.otto..",
    "...otto.otto..",
    "...oBBo.oBBo..",
    "...oBBBooBBBo.",
    "...oooooooooo.",
};

static const char *const LEGS_RUN0[] = { // long stride
    "....otttto....",
    "...ottootto...",
    "..otto..otto..",
    ".otto....otto.",
    ".oBBo....oBBo.",
    "oBBo.....oBBBo",
    "oooo.....ooooo",
};

static const char *const LEGS_RUN1[] = { // passing, front foot up
    "....otttto....",
    "....ottotto...",
    "....ottooBBo..",
    "....otto.ooo..",
    "....oBBo......",
    "....oBBBo.....",
    "....ooooo.....",
};

static const char *const LEGS_RUN2[] = { // short stride
    "....otttto....",
    "...ottootto...",
    "...otto.otto..",
    "..otto...otto.",
    "..oBBo...oBBo.",
    ".oBBo....oBBBo",
    ".oooo....ooooo",
};

static const char *const LEGS_RUN3[] = { // passing, back foot up
    "....otttto....",
    "...ottotto....",
    "..oBBootto....",
    "..ooo.otto....",
    "......oBBo....",
    "......oBBBo...",
    "......ooooo...",
};

static const char *const LEGS_JUMP[] = { // tucked
    "....otttto....",
    "...ottootto...",
    "...otto.otto..",
    "...oBBo.oBBo..",
    "...oBBBooBBBo.",
    "...oooooooooo.",
    "..............",
};

static const char *const LEGS_FALL[] = { // dangling
    "....otttto....",
    "...ottootto...",
    "..otto..otto..",
    "..otto..otto..",
    "..oBBo..oBBo..",
    "..oBBBo.oBBBo.",
    "..ooooo.ooooo.",
};

static const char *const LEGS_CLIMB0[] = {
    "....otttto....",
    "....ottotto...",
    "....oBBootto..",
    "....oooootto..",
    "........oBBo..",
    "........oBBo..",
    "........oooo..",
};

static const char *const LEGS_CLIMB1[] = {
    "....otttto....",
    "...ottotto....",
    "..ottooBBo....",
    "..ottoooo.....",
    "..oBBo........",
    "..oBBo........",
    "..oooo........",
};

static const char *const ARM[] = { // shoulder on the left, hand on the right
    ".ooooo.",
    "oTTTSSo",
    "otttSSo",
    ".ooooo.",
};

static const char *const PUNCH[] = { // arm thrown forward, big fist
    "......ooo.",
    ".oooooSSSo",
    "oTTTTSSSSo",
    ".oooooSSSo",
    "......ooo.",
};

static const char *const PISTOL[] = {
    ".oooooo.",
    "oMMMMMMo",
    "ommmmmo.",
    "omGo....",
    "ooo.....",
};

static const char *const UZI[] = {
    ".ooooooo...",
    "oMMMMMMMooo",
    "ommmmmmmo..",
    ".omGo.omo..",
    ".ooo..omo..",
    "......ooo..",
};

static const char *const GRENADE[] = {
    "..oo..",
    ".oMMo.",
    "oVVVVo",
    "oVvVvo",
    "ovVvVo",
    ".oooo.",
};

static const char *const ROCKET_LAUNCHER[] = {
    ".oooooooooooooo.",
    "omVVVVVVVVVVVVmo",
    "omvvvvvvvvvvvvmo",
    ".oooooooooooooo.",
    "......omo.......",
    "......ooo.......",
};

static const char *const ROCKET[] = {
    "oo.oooooo.",
    "oMoMMMMMRo",
    ".ommmmmmRo",
    "oMoMMMMMRo",
    "oo.oooooo.",
};

static const char *const CRATE[] = {
    "oooooooooooooooo",
    "owMwwwwwwwwwwMwo",
    "owwddddddddddwwo",
    "owdwCCCCCCCCCdwo",
    "owdcwCCCCCCCCdwo",
    "owdCcwCCCCCCCdwo",
    "owdCCcwCCCCCCdwo",
    "owdCCCcwCCCCCdwo",
    "owdCCCCcwCCCCdwo",
    "owdCCCCCcwCCCdwo",
    "owdCCCCCCcwCCdwo",
    "owdCCCCCCCcwCdwo",
    "owdCCCCCCCCcwdwo",
    "owwddddddddddwwo",
    "owMwwwwwwwwwwMwo",
    "oooooooooooooooo",
};

static const char *const PARACHUTE[] = {
    ".....oooooooooo.....",
    "...ooWWWYYYYWWWoo...",
    "..oWWWWYYYYYYWWWWo..",
    ".oWWWWYYYYYYYYWWWWo.",
    ".oooooooooooooooooo.",
    "..o..............o..",
    "..o..............o..",
    "..o..............o..",
    "..o..............o..",
    "..o..............o..",
};

static const char *const STAR[] = {
    "..Y..",
    ".YYY.",
    "YYWYY",
    ".YYY.",
    "..Y..",
};

static const Part PARTS[] = {
    PART(head, 14, HEAD),
    PART(head_striped, 14, HEAD_STRIPED),
    PART(torso, 12, TORSO),
    PART(torso_strong, 12, TORSO_STRONG),
    PART(torso_heavy, 14, TORSO_HEAVY),
    PART(legs_idle, 14, LEGS_IDLE),
    PART(legs_run0, 14, LEGS_RUN0),
    PART(legs_run1, 14, LEGS_RUN1),
    PART(legs_run2, 14, LEGS_RUN2),
    PART(legs_run3, 14, LEGS_RUN3),
    PART(legs_jump, 14, LEGS_JUMP),
    PART(legs_fall, 14, LEGS_FALL),
    PART(legs_climb0, 14, LEGS_CLIMB0),
    PART(legs_climb1, 14, LEGS_CLIMB1),
    PART(arm, 7, ARM),
    PART(punch, 10, PUNCH),
    PART(pistol, 8, PISTOL),
    PART(uzi, 11, UZI),
    PART(grenade, 6, GRENADE),
    PART(rocket_launcher, 16, ROCKET_LAUNCHER),
    PART(rocket, 10, ROCKET),
    PART(crate, 16, CRATE),
    PART(parachute, 20, PARACHUTE),
    PART(star, 5, STAR),
};

enum {
    P_HEAD,
    P_HEAD_STRIPED,
    P_TORSO,
    P_TORSO_STRONG,
    P_TORSO_HEAVY,
    P_LEGS_IDLE,
    P_LEGS_RUN0,
    P_LEGS_RUN1,
    P_LEGS_RUN2,
    P_LEGS_RUN3,
    P_LEGS_JUMP,
    P_LEGS_FALL,
    P_LEGS_CLIMB0,
    P_LEGS_CLIMB1,
    P_ARM,
    P_PUNCH,
    P_PISTOL,
    P_UZI,
    P_GRENADE,
    P_ROCKET_LAUNCHER,
    P_ROCKET,
    P_CRATE,
    P_PARACHUTE,
    P_STAR,
    P_COUNT
};

_Static_assert(sizeof(PARTS) / sizeof(PARTS[0]) == P_COUNT, "one part per enum entry");

// ---- Layout ------------------------------------------------------------------------------------

static const i32 POSE_BOB[POSE_COUNT] = {
    [POSE_IDLE0] = 0, [POSE_IDLE1] = 1, [POSE_RUN0] = 1, [POSE_RUN1] = 0, [POSE_RUN2] = 1,
    [POSE_RUN3] = 0,  [POSE_JUMP] = 0,  [POSE_FALL] = 0, [POSE_CLIMB0] = 0, [POSE_CLIMB1] = 1,
};

static const i32 POSE_LEGS[POSE_COUNT] = {
    [POSE_IDLE0] = P_LEGS_IDLE, [POSE_IDLE1] = P_LEGS_IDLE, [POSE_RUN0] = P_LEGS_RUN0, [POSE_RUN1] = P_LEGS_RUN1,
    [POSE_RUN2] = P_LEGS_RUN2,  [POSE_RUN3] = P_LEGS_RUN3,  [POSE_JUMP] = P_LEGS_JUMP, [POSE_FALL] = P_LEGS_FALL,
    [POSE_CLIMB0] = P_LEGS_CLIMB0, [POSE_CLIMB1] = P_LEGS_CLIMB1,
};

i32 sprite_pose_bob(BodyPose pose) {
    return pose >= 0 && pose < POSE_COUNT ? POSE_BOB[pose] : 0;
}

// The single part a non-body sprite is made of.
static i32 sprite_part(SpriteId id) {
    static const i32 parts[] = {P_ARM,    P_PUNCH, P_PISTOL, P_UZI,       P_GRENADE, P_ROCKET_LAUNCHER,
                                P_ROCKET, P_CRATE, P_CRATE,  P_PARACHUTE, P_STAR};
    return parts[id - SPR_ARM];
}

SpriteInfo sprite_info(SpriteId id) {
    if (id < SPR_ARM) {
        return (SpriteInfo){SPRITE_BODY_W, SPRITE_BODY_H, {(f32)SPRITE_BODY_FEET_X, (f32)SPRITE_BODY_H}};
    }
    const Part *p = &PARTS[sprite_part(id)];
    Vec2 pivot = vec2((f32)p->w * 0.5f, (f32)p->h * 0.5f);
    switch (id) {
    case SPR_ARM:
        pivot = vec2(1.5f, 2.0f); // the shoulder
        break;
    case SPR_PUNCH:
        pivot = vec2(1.0f, 2.5f);
        break;
    case SPR_PISTOL:
        pivot = vec2(2.5f, 3.5f); // the grip
        break;
    case SPR_UZI:
        pivot = vec2(2.5f, 3.5f);
        break;
    case SPR_ROCKET_LAUNCHER:
        pivot = vec2(7.5f, 4.5f);
        break;
    default:
        break;
    }
    return (SpriteInfo){p->w, p->h, pivot};
}

Rect sprite_rect(SpriteId id) {
    const SpriteInfo info = sprite_info(id);
    const i32 cell = (i32)id;
    return (Rect){(f32)((cell % SPRITE_COLUMNS) * SPRITE_CELL), (f32)((cell / SPRITE_COLUMNS) * SPRITE_CELL), (f32)info.w,
                  (f32)info.h};
}

void sprites_atlas_size(i32 *w, i32 *h) {
    *w = SPRITE_COLUMNS * SPRITE_CELL;
    *h = ((SPR_COUNT + SPRITE_COLUMNS - 1) / SPRITE_COLUMNS) * SPRITE_CELL;
}

// ---- Building ----------------------------------------------------------------------------------

typedef struct Painter {
    u8 *rgba;
    i32 stride; // atlas width in pixels
    u8 team[3], team_dark[3], team_light[3];
    bool flash;
} Painter;

static void color_of(const Painter *p, char key, u8 out[3]) {
    if (p->flash) {
        out[0] = out[1] = out[2] = 255;
        return;
    }
    const u8 *src = NULL;
    if (key == 'T') {
        src = p->team;
    } else if (key == 't') {
        src = p->team_dark;
    } else if (key == 'L') {
        src = p->team_light;
    }
    if (src) {
        memcpy(out, src, 3);
        return;
    }
    for (usize i = 0; i < sizeof(FIXED_COLORS) / sizeof(FIXED_COLORS[0]); i++) {
        if (FIXED_COLORS[i].key == key) {
            out[0] = FIXED_COLORS[i].r;
            out[1] = FIXED_COLORS[i].g;
            out[2] = FIXED_COLORS[i].b;
            return;
        }
    }
    out[0] = 255, out[1] = 0, out[2] = 255; // unknown: loud magenta (sprites_validate catches it)
}

// `swap`: pairs of characters "from to from to..." applied before the palette.
static char swapped(char key, const char *swap) {
    for (const char *s = swap; s && s[0] && s[1]; s += 2) {
        if (s[0] == key) {
            return s[1];
        }
    }
    return key;
}

// Draws a part with its top-left corner at (x, y) of the cell of `id`, clipped to the cell.
static void stamp(const Painter *p, SpriteId id, i32 part, i32 x, i32 y, const char *swap) {
    const Part *pt = &PARTS[part];
    const Rect cell = sprite_rect(id);
    for (i32 row = 0; row < pt->h; row++) {
        for (i32 col = 0; col < pt->w; col++) {
            const char key = swapped(pt->rows[row][col], swap);
            const i32 px = x + col, py = y + row;
            if (key == '.' || px < 0 || py < 0 || px >= SPRITE_CELL || py >= SPRITE_CELL) {
                continue;
            }
            u8 *dst = p->rgba + (((i32)cell.y + py) * p->stride + (i32)cell.x + px) * 4;
            color_of(p, key, dst);
            dst[3] = 255;
        }
    }
}

static const char ARMORED_SWAP[] = "TMtmLWGT"; // steel instead of cloth, the team color moves to the belt

static void build_body(const Painter *p, i32 variant, BodyPose pose) {
    const SpriteId id = sprite_body(variant, pose);
    const i32 bob = POSE_BOB[pose];
    stamp(p, id, POSE_LEGS[pose], 1, 17, NULL);
    switch (variant) {
    case 1: // strong
        stamp(p, id, P_TORSO_STRONG, 3, 10 + bob, NULL);
        stamp(p, id, P_HEAD_STRIPED, 1, bob, NULL);
        break;
    case 2: // heavy
        stamp(p, id, P_TORSO_HEAVY, 2, 10 + bob, NULL);
        stamp(p, id, P_HEAD, 1, bob, NULL);
        break;
    case 3: // heavy armored
        stamp(p, id, P_TORSO_HEAVY, 2, 10 + bob, ARMORED_SWAP);
        stamp(p, id, P_HEAD, 1, bob, "TMtmLW");
        break;
    default:
        stamp(p, id, P_TORSO, 3, 10 + bob, NULL);
        stamp(p, id, P_HEAD, 1, bob, NULL);
        break;
    }
}

static u8 lighten(u8 c) {
    return (u8)(c + (255 - c) * 2 / 5);
}

void sprites_build_atlas(u8 *rgba, const u8 team[3], const u8 team_dark[3], bool flash) {
    i32 w, h;
    sprites_atlas_size(&w, &h);
    memset(rgba, 0, (usize)w * (usize)h * 4u);
    Painter p = {.rgba = rgba, .stride = w, .flash = flash};
    memcpy(p.team, team, 3);
    memcpy(p.team_dark, team_dark, 3);
    for (int k = 0; k < 3; k++) {
        p.team_light[k] = lighten(team[k]);
    }
    for (i32 v = 0; v < SPRITE_VARIANTS; v++) {
        for (i32 pose = 0; pose < POSE_COUNT; pose++) {
            build_body(&p, v, (BodyPose)pose);
        }
    }
    for (i32 id = SPR_ARM; id < SPR_COUNT; id++) {
        const char *swap = id == SPR_CRATE_BLUE ? "CUcu" : NULL;
        stamp(&p, (SpriteId)id, sprite_part((SpriteId)id), 0, 0, swap);
    }
}

bool sprites_validate(char *err, usize err_len) {
    for (i32 i = 0; i < P_COUNT; i++) {
        const Part *p = &PARTS[i];
        if (p->w > SPRITE_CELL || p->h > SPRITE_CELL) {
            snprintf(err, err_len, "%s: larger than a cell", p->name);
            return false;
        }
        for (i32 row = 0; row < p->h; row++) {
            if ((i32)strlen(p->rows[row]) != p->w) {
                snprintf(err, err_len, "%s: row %d is %d wide, not %d", p->name, (int)row, (int)strlen(p->rows[row]),
                         (int)p->w);
                return false;
            }
            for (i32 col = 0; col < p->w; col++) {
                if (!known_key(p->rows[row][col])) {
                    snprintf(err, err_len, "%s: unknown color '%c' (row %d)", p->name, p->rows[row][col], (int)row);
                    return false;
                }
            }
        }
    }
    return true;
}
