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
//  x  wood, light        y  straw, sandbag        z  straw, dark
//  A  stone              a  stone, dark           q  stone, light
//  g  grass              h  grass, dark           l  grass, light
//  D  dirt               e  dirt, dark            E  dirt, light    p  pebble          F  flower

typedef struct PaletteEntry {
    char key;
    u8 r, g, b;
} PaletteEntry;

static const PaletteEntry FIXED_COLORS[] = {
    {'o', 34, 28, 42},    {'S', 244, 196, 150}, {'s', 206, 150, 112}, {'W', 250, 250, 250}, {'K', 24, 24, 32},
    {'B', 70, 58, 56},    {'G', 126, 96, 58},   {'Y', 255, 214, 80},  {'M', 176, 182, 196}, {'m', 112, 118, 134},
    {'V', 110, 134, 70},  {'v', 74, 92, 46},    {'w', 176, 124, 66},  {'d', 112, 74, 38},   {'R', 220, 50, 40},
    {'C', 210, 62, 52},   {'c', 140, 36, 32},   {'U', 60, 112, 228},  {'u', 34, 66, 150},   {'x', 214, 166, 100},
    {'y', 214, 186, 120}, {'z', 160, 128, 78},  {'A', 134, 130, 146}, {'a', 88, 84, 102},   {'q', 178, 176, 192},
    {'g', 108, 178, 84},  {'h', 62, 120, 60},   {'l', 158, 214, 104}, {'D', 118, 84, 74},   {'e', 94, 66, 62},
    {'E', 140, 102, 88},  {'p', 164, 128, 106}, {'F', 240, 120, 160},
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

static const char *const DUMMY[] = { // a straw training dummy with a target on its chest
    ".....oooo.....",
    "....oyyyyo....",
    "...oyyyyyyo...",
    "...oyzyyzyo...",
    "...oyyyyyyo...",
    "...oyzzzzyo...",
    "....oyyyyo....",
    ".oooooddooooo.",
    "oddddddddddddo",
    "ooooyyyyyyoooo",
    "...oyWWWWyo...",
    "...oWRRRRWo...",
    "...oWRWWRWo...",
    "...oWRWWRWo...",
    "...oWRRRRWo...",
    "...oyWWWWyo...",
    "...oyyzyyyo...",
    "...ozyzyzzo...",
    "....oodwoo....",
    ".....odwo.....",
    ".....odwo.....",
    ".....odwo.....",
    "...ooddwwoo...",
    "..oooooooooo..",
};

static const char *const TANK[] = {
    "......oooo......",
    ".....oLTTTo.....",
    ".....oTTTToooooo",
    ".....oTTTTMMMMMo",
    "..oooottttoooooo",
    ".oLLLLLLLLLLLLo.",
    "oTTTTTTTTTTTTTTo",
    "otttttttttttttto",
    "oBmBmBmBmBmBmBmo",
    ".oBBBBBBBBBBBBo.",
    "..oooooooooooo..",
};

static const char *const HEAVY_TANK[] = { // steel plates and a thicker gun
    "......ooooo.......",
    ".....oMLTTTo......",
    ".....oMTTTTooooooo",
    ".....oMTTTTMMMMMMo",
    ".....oMTTTTmmmmmmo",
    "..oooottttoooooooo",
    ".oMMLLLLLLLLLLMMo.",
    "oMMTTTTTTTTTTTTMMo",
    "otttttttttttttttto",
    "oBmBmBmBmBmBmBmBmo",
    ".oBBBBBBBBBBBBBBo.",
    "..oooooooooooooo..",
};

static const char *const CASE[] = {
    "....oooo....",
    "...o....o...",
    "oooooooooooo",
    "oVVVVMMVVVVo",
    "ovvvvvvvvvvo",
    "oooooooooooo",
};

static const char *const FLAG0[] = {
    "YY.........",
    "Mmooooooo..",
    "MmoLTTTTTo.",
    "MmoTTTTTTTo",
    "MmoTTTTTTto",
    "Mmottttoooo",
    "Mmoooo.....",
    "Mm.........",
    "Mm.........",
    "Mm.........",
    "Mm.........",
    "Mm.........",
    "Mm.........",
    "Mm.........",
    "Mm.........",
    "Mm.........",
};

static const char *const FLAG1[] = {
    "YY.........",
    "Mmoooo.....",
    "MmoLTToooo.",
    "MmoTTTTTTTo",
    "MmoTTTTTTTo",
    "MmotttTTtto",
    "Mmooooooooo",
    "Mm.........",
    "Mm.........",
    "Mm.........",
    "Mm.........",
    "Mm.........",
    "Mm.........",
    "Mm.........",
    "Mm.........",
    "Mm.........",
};

// Terrain: no outline, the tiles must join without seams.
static const char *const DIRT0[] = {
    "DDDDDDDDDDDDDDDD",
    "DDDDDDDDDDDEDDDD",
    "DDeDDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDD",
    "DDDDDDDEpDDDDDDD",
    "DDDDDDEppeDDDDeD",
    "DDDDDDDeeDDDDDDD",
    "DEDDDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDD",
    "DDDDDDDDDDDDeDDD",
    "DDDDDDDDDDDDDDDD",
    "DDDDeDDDDDDDDDDD",
    "DDDDDDDDDDDDEpDD",
    "DDDDDDDDDDDEppeD",
    "DDEDDDDDDDDDeeDD",
    "DDDDDDDDDDDDDDDD",
};

static const char *const DIRT1[] = {
    "DDDDDDDDDDDDDDDD",
    "DDDDDeDDDDDDDDDD",
    "DDDDDDDDDDDDDEDD",
    "DDDDDDDDDDDDDDDD",
    "DDEpDDDDDDDDDDDD",
    "DEppeDDDDDDeDDDD",
    "DDeeDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDD",
    "DDDDDDDDEDDDDDDD",
    "DDDDDDDDDDDDDDeD",
    "DDDDDDDDDDDDDDDD",
    "DDDDDDDEpDDDDDDD",
    "DDeDDDEppeDDDDDD",
    "DDDDDDDeeDDDDDDD",
    "DDDDDDDDDDDDDEDD",
    "DDDDDDDDDDDDDDDD",
};

static const char *const DIRT2[] = {
    "DDDDDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDD",
    "DDDDDDDDDeDDDDDD",
    "DDDDEDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDD",
    "DDDDDDDDDDDEDDDD",
    "DDeDDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDD",
    "DDDDDDEpppDDDDDD",
    "DDDDDEppppeDDDDD",
    "DDDDDDeeeeDDDeDD",
    "DDDDDDDDDDDDDDDD",
    "DEDDDDDDDDDDDDDD",
    "DDDDDDDDDDEDDDDD",
    "DDDDDDDDDDDDDDDD",
};

static const char *const GRASS_TOP0[] = { // stamped over a dirt tile; '.' lets the dirt show (ragged edge)
    "llllllllllllllll",
    "glgggglgggggglgg",
    "gggggggggggggggg",
    "ggghggggghgggggh",
    "hghhhghhhhhghhhh",
    "hh.h.hh..h.hh.hh",
    ".h....h.....h...",
};

static const char *const GRASS_TOP1[] = {
    "llllllllllllllll",
    "gggglgggglgggggl",
    "gggggggggggggggg",
    "hggggghgggggghgg",
    "hhhghhhhghhhhhgh",
    "h.hhh.h.hh.h.hhh",
    "..h.....h.....h.",
};

static const char *const PLANK[] = {
    "xxxxxxxxxxxxxxxx",
    "wwdwwwwdwwwwwdww",
    "wwwwwwwdwwwwwwww",
    "wwwwwwwdwwwwwwww",
    "dwwwwwwdwwwwwwwd",
    "dddddddddddddddd",
};

static const char *const LADDER[] = {
    "..dw........dw..",
    "..dwxxxxxxxxdw..",
    "..dwdddddddddw..",
    "..dw........dw..",
    "..dw........dw..",
    "..dwxxxxxxxxdw..",
    "..dwdddddddddw..",
    "..dw........dw..",
    "..dw........dw..",
    "..dwxxxxxxxxdw..",
    "..dwdddddddddw..",
    "..dw........dw..",
    "..dw........dw..",
    "..dwxxxxxxxxdw..",
    "..dwdddddddddw..",
    "..dw........dw..",
};

static const char *const TUFT[] = {
    ".l...l.",
    ".g.l.g.",
    "lg.g.gl",
    "ghgghgh",
};

static const char *const FLOWER[] = {
    ".F.",
    "FYF",
    ".F.",
    ".g.",
    "gg.",
    ".g.",
};

static const char *const ROCK[] = {
    "..aaa..",
    ".aqAAa.",
    "aqAAAAa",
    "aaaaaaa",
};

static const char *const HEART[] = {
    ".oo.oo.",
    "oRWRRRo",
    "oRRRRRo",
    ".oRRRo.",
    "..oRo..",
    "...o...",
};

static const char *const AMMO[] = { // a bullet, brass case and silver tip
    "..oo..",
    ".oMMo.",
    ".oMmo.",
    ".oYYo.",
    ".oYYo.",
    ".oYYo.",
    "..oo..",
};

// The base, in three states of damage, and what is left of it. Team colors on the walls.
static const char *const BASE[] = {
    ".....oooooooooooooooooooooooooooooo.....",
    "....ozyyyyyzyyyyyyzyyyyyyzyyyyyyzyyo....",
    "....oyyyyyyzyyyyyyzyyyyyyzyyyyyyzyyo....",
    "....oyyyyyyzyyyyyyzyyyyyyzyyyyyyzyyo....",
    "...oozzzzzzzzzzzzzzzzzzzzzzzzzzzzzzoo...",
    "..ozyyzyyyyyyzyyyyyyzyyyyyyzyyyyyyzyyo..",
    "..oyyyzyyyyyyzyyyyyyzyyyyyyzyyyyyyzyyo..",
    "..oyyyzyyyyyyzyyyyyyzyyyyyyzyyyyyyzyyo..",
    ".oozzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzoo.",
    "oqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo",
    "oAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAo",
    "oaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaao",
    ".oottttttttttttttttttttttttttttttttttoo.",
    "..ottttttttttttttttWWtttttttttttttttto..",
    "..oLTTTTTTTTTTTTTTWWWWTTTTTTTTTTTTTTto..",
    "..oLTTooooooTTTTTTWWWWTTTTTTooooooTTto..",
    "..oLTToWWWWoTTTTTTTWWTTTTTTToWWWWoTTto..",
    "..oLTToYYYYoTTTTTTTTTTTTTTTToYYYYoTTto..",
    "..oLttoooooottttttttttttttttoooooottto..",
    "..oLTToYYYYoTTTooooooooooTTToYYYYoTTto..",
    "..oLTTooooooTTToddddddddoTTTooooooTTto..",
    "..oLTTTaaaaTTTTodwdwdwddoTTTTaaaaTTTto..",
    "..oLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLtttttttttttodwdwdwddotttttttttttto..",
    "..oLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLTTTTTTTTTTTodwdwdwYdoTTTTTTTTTTTto..",
    "..oLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLtttttttttttodwdwdwddotttttttttttto..",
    "..oLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "oooLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTtooo",
    "qqqqqqqqqqqqqqqAAAAAAAAAAqqqqqqqqqqqqqqq",
    "aAAAAAAAaAAAAAAaaaaaaaaaaAAAAAAAaAAAAAAA",
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "AAAAaAAAAAAAaAAaaaaaaaaaaAAAaAAAAAAAaAAA",
    "AAAAaAAAAAAAaAAaaaaaaaaaaAAAaAAAAAAAaAAA",
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
};

static const char *const BASE_DAMAGED[] = {
    ".....ooooooooooooooooooooo.......oo.....",
    "....ozyyyyyzyyyyyyzyyyyyyzo.....oyyo....",
    "....oyyyyyyzyyyyyyzyyyyyyzo.....oyyo....",
    "....oyyyyyyzyyyyyyzyyyyyyzo.....oyyo....",
    "...oozzzzzzzzzzzzzzzzzzzzzooooooozzoo...",
    "..ozyyzyyyyyyzyyyyyyzyyyyyyzyyyyyyzyyo..",
    "..oyyyzyyyyyyzyyyyyyzyyyyyyzyyyyyyzyyo..",
    "..oyyyzyyyyyyzyyyyyyzyyyyyyzyyyyyyzyyo..",
    ".oozzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzoo.",
    "oqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo",
    "oAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAo",
    "oaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaao",
    ".oottttttttttttttttttttttttttttttttttoo.",
    "..ottttttttttttttttWWtttttttttttotttto..",
    "..oLTTTTTTTTTTTTTTWWWWTTTTTTTTToTTTTto..",
    "..oLTTooooooTTTTTTWWWWTTTTTTooooooTTto..",
    "..oLTToWWWWoTTTTTTTWWTTTTTTToaaaaoTTto..",
    "..oLTToYYYYoTTTTTTTTTTTTTTTToaaaaoTTto..",
    "..oLttoooooottttttttttttttttoooooottto..",
    "..oLTToYYYYoTTTooooooooooTTToYYYYoTTto..",
    "..oLTTooooooTTToddddddddoTTTooooooTTto..",
    "..oLTTTaaaaTTTTodwdwdwddoTTTTaaaaTTTto..",
    "..oLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLttttottttttodwdwdwddotttttttttttto..",
    "..oLTTTTToTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLTTTTToTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLTTTTTToTTTTodwdwdwYdoTTTTTTTTTTTto..",
    "..oLTTTTTToTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLTTTTTTToTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLtttttttttttodwdwdwddotttttttttttto..",
    "..oLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "oooLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTtooo",
    "qqqqqqqqqqqqqqqAAAAAAAAAAqqqqqqqqqqqqqqq",
    "aAAAAAAAaAAAAAAaaaaaaaaaaAAAAAAAaAAAAAAA",
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "AAAAaAAAAAAAaAAaaaaaaaaaaAAAaAAAAAAAaAAA",
    "AAAAaAAAAAAAaAAaaaaaaaaaaAAAaAAAAAAAaAAA",
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
};

static const char *const BASE_WRECKED[] = {
    ".....ooooooooooooooooooooo.......oo.....",
    "....ozyyyyyzyyyyyyzyyyyyyzo.....oyyo....",
    "....oyyyyyyzyyyyyyzyyyyyyzo.....oyyo....",
    "....oyyyyyyzyyyyyyzyyyyyyzo.....oyyo....",
    "...oozzzzzzzzzzzzzzzzzzzzzooooooozzoo...",
    "..ozyyoooooooooyyyyyzyyyyyyzyyyyyyzyyo..",
    "..oyyyo.......oyyyyyzyyyyyyzyyyyyyzyyo..",
    "..oyyyo.......oyyyyyzyyyyyyzyyyyyyzyyo..",
    ".oozzzo.......ozzzzzzzzzzzzzzzzzzzzzzoo.",
    "oqqqqqoooooooooqqqqqqqqqqqqqqqqqqqqqqqqo",
    "oAAAAAoooooooooAAAAAAAAAAAAAAAAAAAAAAAAo",
    "oaaaaaoooooooooaaaaaaaaaaaaaaaaaaaaaaaao",
    ".oottttoooooootttttttttttttttttttttttoo.",
    "..ottttttttttttttttWWtttttttttttotttto..",
    "..oLTTTTTTTTTTTTTTWWWWTTTTTTTTToTTTTto..",
    "..oLTTooooooTTTTTTWWWWTTTTTTooooooTTto..",
    "..oLoTooooooTTTTTTTWWTTTTTTToaaaaoTTto..",
    "..oLToooooooTTTTTTTTTTTTTTTToaaaaoTTto..",
    "..oLtooooooottttttttttttttttoooooottto..",
    "..oLTTooooooTTTooooooooooTTToYYYYoTTto..",
    "..oLTTooooooTTToddddddddoTTTooooooTTto..",
    "..oLTTTaaaaTTTTodwdwdwddoTTTTaaaaTTTto..",
    "..oLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLTTTTTTTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLttttottttttodwdwdwddotttttttttttto..",
    "..oLTTTTToTTTTTodwdwdwddoTTTTTTTTTTTto..",
    "..oLTTTTToTTTTTodwdwdwddoToTTTTTTTTTto..",
    "..oLTTTTTToTTTTodwdwdwYdoTToTTTTTTTTto..",
    "..oLTTTTTToTTTTodwdwdwddoTToTTTTTTTTto..",
    "..oLTTTTTTToTTTodwdwdwddoTTToTTTTTTTto..",
    "..oLtttttttttttodwdwdwddtttttottttttto..",
    "..oLTTTTTTTTtTTodwdwdwddotTToTTTTTTTto..",
    "..oLTTTTTTTTTtTodwdwdwdtoTTTTTTTTTTTto..",
    "oooLTTTTTTTtTTTodwdwdwddoTtTTTTTTTTTtooo",
    "qqqqqqqqqqqqqqqAAAAAAAAAAqqqqqqqqqqqqqqq",
    "aAAAAAAAaAAAAAAaaaaaaaaaaAAAAAAAaAAAAAAA",
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "AAAAaAAAAAAAaAAaaaaaaaaaaAAAaAAAAAAAaAAA",
    "AAAAaAAAAAAAaAAaaaaaaaaaaAAAaAAAAAAAaAAA",
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
};

static const char *const BASE_RUIN[] = {
    ".....oMo................................",
    ".....oMo................................",
    "......oMo.................oo............",
    "......oMo................oAAo...........",
    ".......oMo...............oAAo...........",
    "......ooMo.......ooo.....oAAo...........",
    ".....oTTTTo.....odddo.ooooAAoo..........",
    ".....oTTTTo.....odddootLLLLLLto..oo.....",
    "...oooTTTTooo...odddootttttttto.oyyo....",
    "..otTTTTTTTTtoooodddoottttttttoooyyoo...",
    "..ottttttttttoyyyyyyyottttttttqqqqqqqo..",
    "..ottttttttttoyyyyyyyottttttttAAAAAAAo..",
    "..ottttttttttoyyyyyyyottttttttAAAAAAAo..",
    ".oottttttttttozzzzzzzottttttttAAAAAAAoo.",
    "oqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo",
    "oaAAAAAAAaAAAAAAAaAAAAAAAaAAAAAAAaAAAAAo",
    "oaAAAAAAAaAAAAAAAaAAAAAAAaAAAAAAAaAAAAAo",
    "oaAAAAAAAaAAAAAAAaAAAAAAAaAAAAAAAaAAAAAo",
    "oaAAAAAAAaAAAAAAAaAAAAAAAaAAAAAAAaAAAAAo",
    "oaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaao",
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
    PART(dummy, 14, DUMMY),
    PART(tank, 16, TANK),
    PART(heavy_tank, 18, HEAVY_TANK),
    PART(case, 12, CASE),
    PART(flag0, 11, FLAG0),
    PART(flag1, 11, FLAG1),
    PART(dirt0, 16, DIRT0),
    PART(dirt1, 16, DIRT1),
    PART(dirt2, 16, DIRT2),
    PART(grass_top0, 16, GRASS_TOP0),
    PART(grass_top1, 16, GRASS_TOP1),
    PART(plank, 16, PLANK),
    PART(ladder, 16, LADDER),
    PART(tuft, 7, TUFT),
    PART(flower, 3, FLOWER),
    PART(rock, 7, ROCK),
    PART(heart, 7, HEART),
    PART(ammo, 6, AMMO),
    PART(base, 40, BASE),
    PART(base_damaged, 40, BASE_DAMAGED),
    PART(base_wrecked, 40, BASE_WRECKED),
    PART(base_ruin, 40, BASE_RUIN),
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
    P_DUMMY,
    P_TANK,
    P_HEAVY_TANK,
    P_CASE,
    P_FLAG0,
    P_FLAG1,
    P_DIRT0,
    P_DIRT1,
    P_DIRT2,
    P_GRASS_TOP0,
    P_GRASS_TOP1,
    P_PLANK,
    P_LADDER,
    P_TUFT,
    P_FLOWER,
    P_ROCK,
    P_HEART,
    P_AMMO,
    P_BASE,
    P_BASE_DAMAGED,
    P_BASE_WRECKED,
    P_BASE_RUIN,
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

SpriteId sprite_for_weapon(WeaponKind weapon) {
    switch (weapon) {
    case WEAPON_PISTOL:
        return SPR_PISTOL;
    case WEAPON_UZI:
        return SPR_UZI;
    case WEAPON_GRENADES:
    case WEAPON_STRONG_GRENADES:
        return SPR_GRENADE;
    case WEAPON_ROCKET_LAUNCHER:
        return SPR_ROCKET_LAUNCHER;
    default:
        return SPR_COUNT;
    }
}

i32 sprite_pose_bob(BodyPose pose) {
    return pose >= 0 && pose < POSE_COUNT ? POSE_BOB[pose] : 0;
}

// How a non-body sprite is made: a part, optionally a second one stamped over it, and a color swap.
typedef struct SpriteRecipe {
    i32 part, overlay; // overlay < 0: none
    const char *swap;
} SpriteRecipe;

static const SpriteRecipe RECIPES[SPR_COUNT - SPR_ARM] = {
    [SPR_ARM - SPR_ARM] = {P_ARM, -1, NULL},
    [SPR_PUNCH - SPR_ARM] = {P_PUNCH, -1, NULL},
    [SPR_PISTOL - SPR_ARM] = {P_PISTOL, -1, NULL},
    [SPR_UZI - SPR_ARM] = {P_UZI, -1, NULL},
    [SPR_GRENADE - SPR_ARM] = {P_GRENADE, -1, NULL},
    [SPR_ROCKET_LAUNCHER - SPR_ARM] = {P_ROCKET_LAUNCHER, -1, NULL},
    [SPR_ROCKET - SPR_ARM] = {P_ROCKET, -1, NULL},
    [SPR_CRATE_RED - SPR_ARM] = {P_CRATE, -1, NULL},
    [SPR_CRATE_BLUE - SPR_ARM] = {P_CRATE, -1, "CUcu"},
    [SPR_PARACHUTE - SPR_ARM] = {P_PARACHUTE, -1, NULL},
    [SPR_STAR - SPR_ARM] = {P_STAR, -1, NULL},
    [SPR_DUMMY - SPR_ARM] = {P_DUMMY, -1, NULL},
    [SPR_TANK - SPR_ARM] = {P_TANK, -1, NULL},
    [SPR_HEAVY_TANK - SPR_ARM] = {P_HEAVY_TANK, -1, NULL},
    [SPR_CASE - SPR_ARM] = {P_CASE, -1, NULL},
    [SPR_FLAG0 - SPR_ARM] = {P_FLAG0, -1, NULL},
    [SPR_FLAG1 - SPR_ARM] = {P_FLAG1, -1, NULL},
    [SPR_TILE_GRASS0 - SPR_ARM] = {P_DIRT0, P_GRASS_TOP0, NULL},
    [SPR_TILE_GRASS1 - SPR_ARM] = {P_DIRT1, P_GRASS_TOP1, NULL},
    [SPR_TILE_GRASS2 - SPR_ARM] = {P_DIRT2, P_GRASS_TOP0, "lg"},
    [SPR_TILE_DIRT0 - SPR_ARM] = {P_DIRT0, -1, NULL},
    [SPR_TILE_DIRT1 - SPR_ARM] = {P_DIRT1, -1, NULL},
    [SPR_TILE_DIRT2 - SPR_ARM] = {P_DIRT2, -1, NULL},
    [SPR_TILE_PLANK - SPR_ARM] = {P_PLANK, -1, NULL},
    [SPR_TILE_LADDER - SPR_ARM] = {P_LADDER, -1, NULL},
    [SPR_DECO_TUFT - SPR_ARM] = {P_TUFT, -1, NULL},
    [SPR_DECO_FLOWER - SPR_ARM] = {P_FLOWER, -1, NULL},
    [SPR_DECO_DAISY - SPR_ARM] = {P_FLOWER, -1, "FW"},
    [SPR_DECO_ROCK - SPR_ARM] = {P_ROCK, -1, NULL},
    [SPR_HEART - SPR_ARM] = {P_HEART, -1, NULL},
    [SPR_AMMO - SPR_ARM] = {P_AMMO, -1, NULL},
    [SPR_BASE - SPR_ARM] = {P_BASE, -1, NULL},
    [SPR_BASE_DAMAGED - SPR_ARM] = {P_BASE_DAMAGED, -1, NULL},
    [SPR_BASE_WRECKED - SPR_ARM] = {P_BASE_WRECKED, -1, NULL},
    [SPR_BASE_RUIN - SPR_ARM] = {P_BASE_RUIN, -1, NULL},
};

static i32 sprite_part(SpriteId id) {
    return RECIPES[id - SPR_ARM].part;
}

SpriteInfo sprite_info(SpriteId id) {
    if (id < SPR_ARM) {
        return (SpriteInfo){SPRITE_BODY_W, SPRITE_BODY_H, {(f32)SPRITE_BODY_FEET_X, (f32)SPRITE_BODY_H}};
    }
    const Part *p = &PARTS[sprite_part(id)];
    const f32 w = (f32)p->w, h = (f32)p->h;
    Vec2 pivot = vec2(w * 0.5f, h * 0.5f);
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
    case SPR_FLAG0:
    case SPR_FLAG1:
        pivot = vec2(1.0f, h); // foot of the pole
        break;
    case SPR_TILE_GRASS0:
    case SPR_TILE_GRASS1:
    case SPR_TILE_GRASS2:
    case SPR_TILE_DIRT0:
    case SPR_TILE_DIRT1:
    case SPR_TILE_DIRT2:
    case SPR_TILE_PLANK:
    case SPR_TILE_LADDER:
        pivot = vec2(0.0f, 0.0f); // top left, on the tile grid
        break;
    case SPR_CRATE_RED:
    case SPR_CRATE_BLUE:
    case SPR_DUMMY:
    case SPR_TANK:
    case SPR_HEAVY_TANK:
    case SPR_CASE:
    case SPR_DECO_TUFT:
    case SPR_DECO_FLOWER:
    case SPR_DECO_DAISY:
    case SPR_DECO_ROCK:
    case SPR_BASE:
    case SPR_BASE_DAMAGED:
    case SPR_BASE_WRECKED:
    case SPR_BASE_RUIN:
        pivot = vec2(w * 0.5f, h); // standing on the ground
        break;
    default:
        break;
    }
    return (SpriteInfo){p->w, p->h, pivot};
}

// Rows of small cells, then the big sprites two cells wide and tall.
static i32 big_rows_start(void) {
    return (SPR_BIG_FIRST + SPRITE_COLUMNS - 1) / SPRITE_COLUMNS;
}

static i32 cell_span(SpriteId id) {
    return id >= SPR_BIG_FIRST ? 2 : 1;
}

Rect sprite_rect(SpriteId id) {
    const SpriteInfo info = sprite_info(id);
    i32 col, row;
    if (id >= SPR_BIG_FIRST) {
        const i32 k = (i32)(id - SPR_BIG_FIRST) * 2;
        col = k % SPRITE_COLUMNS;
        row = big_rows_start() + (k / SPRITE_COLUMNS) * 2;
    } else {
        col = (i32)id % SPRITE_COLUMNS;
        row = (i32)id / SPRITE_COLUMNS;
    }
    return (Rect){(f32)(col * SPRITE_CELL), (f32)(row * SPRITE_CELL), (f32)info.w, (f32)info.h};
}

void sprites_atlas_size(i32 *w, i32 *h) {
    const i32 big_cells = (SPR_COUNT - SPR_BIG_FIRST) * 2;
    *w = SPRITE_COLUMNS * SPRITE_CELL;
    *h = (big_rows_start() + ((big_cells + SPRITE_COLUMNS - 1) / SPRITE_COLUMNS) * 2) * SPRITE_CELL;
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
    const i32 span = cell_span(id) * SPRITE_CELL;
    for (i32 row = 0; row < pt->h; row++) {
        for (i32 col = 0; col < pt->w; col++) {
            const char key = swapped(pt->rows[row][col], swap);
            const i32 px = x + col, py = y + row;
            if (key == '.' || px < 0 || py < 0 || px >= span || py >= span) {
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
        const SpriteRecipe *rc = &RECIPES[id - SPR_ARM];
        stamp(&p, (SpriteId)id, rc->part, 0, 0, rc->swap);
        if (rc->overlay >= 0) {
            stamp(&p, (SpriteId)id, rc->overlay, 0, 0, rc->swap);
        }
    }
}

typedef struct RecipeCheck {
    const char *problem; // NULL: fine
} RecipeCheck;

// Every sprite has a recipe, and its parts fit its cells.
static RecipeCheck check_recipe(SpriteId id) {
    const SpriteRecipe *rc = &RECIPES[id - SPR_ARM];
    if (rc->part == P_HEAD && rc->overlay == 0 && !rc->swap) { // zero-filled entry: forgotten in RECIPES
        return (RecipeCheck){"no recipe"};
    }
    const i32 span = cell_span(id) * SPRITE_CELL;
    const Part *a = &PARTS[rc->part];
    if (a->w > span || a->h > span || (rc->overlay >= 0 && (PARTS[rc->overlay].w > span || PARTS[rc->overlay].h > span))) {
        return (RecipeCheck){"larger than its cells"};
    }
    return (RecipeCheck){NULL};
}

bool sprites_validate(char *err, usize err_len) {
    for (i32 i = 0; i < P_COUNT; i++) {
        const Part *p = &PARTS[i];
        if (p->w > 2 * SPRITE_CELL || p->h > 2 * SPRITE_CELL) {
            snprintf(err, err_len, "%s: larger than a big sprite", p->name);
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
    for (i32 id = SPR_ARM; id < SPR_COUNT; id++) {
        const RecipeCheck c = check_recipe((SpriteId)id);
        if (c.problem) {
            snprintf(err, err_len, "sprite %d: %s", (int)id, c.problem);
            return false;
        }
    }
    return true;
}
