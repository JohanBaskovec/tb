// Pixel-art sprites drawn for the project: soldiers (every pose and variant), arms, weapons, projectiles, crates,
// parachute, sparkles. Pure data and image building, no SDL: render.c turns the atlases into textures.
//
// Every sprite is written as text, one character per pixel, from a small palette ('.' is transparent, 'T' / 't' / 'L'
// are the team color, its dark and light shades...). The soldiers are assembled from parts (head, torso, legs) so that
// each pose exists for each variant. Sprites face right; the renderer mirrors them.
//
// An atlas holds every sprite in a grid of SPRITE_CELL x SPRITE_CELL cells. There is one atlas per palette: one per
// team (the team colors differ) and a white one (the silhouette that flashes when a soldier is hit).
#pragma once

#include "core/geom.h"

#define SPRITE_CELL 32
#define SPRITE_COLUMNS 16

// Soldier poses (body without the arms, which are drawn on their own so that they can aim).
typedef enum BodyPose {
    POSE_IDLE0,
    POSE_IDLE1,
    POSE_RUN0,
    POSE_RUN1,
    POSE_RUN2,
    POSE_RUN3,
    POSE_JUMP,
    POSE_FALL,
    POSE_CLIMB0,
    POSE_CLIMB1,
    POSE_COUNT
} BodyPose;

#define SPRITE_VARIANTS 4 // SoldierVariant: normal, strong, heavy, heavy armored

typedef enum SpriteId {
    SPR_BODY_FIRST = 0, // SPR_BODY_FIRST + variant * POSE_COUNT + pose
    SPR_ARM = SPR_BODY_FIRST + SPRITE_VARIANTS * POSE_COUNT,
    SPR_PUNCH,
    SPR_PISTOL,
    SPR_UZI,
    SPR_GRENADE,
    SPR_ROCKET_LAUNCHER,
    SPR_ROCKET,
    SPR_CRATE_RED,
    SPR_CRATE_BLUE,
    SPR_PARACHUTE,
    SPR_STAR,
    SPR_COUNT
} SpriteId;

// Body canvas: the feet are on the bottom row, centered on column BODY_FEET_X.
#define SPRITE_BODY_W 16
#define SPRITE_BODY_H 24
#define SPRITE_BODY_FEET_X 8
#define SPRITE_SHOULDER_X 8  // in the body canvas (facing right)
#define SPRITE_SHOULDER_Y 12 // plus the pose's bob

typedef enum SpritePaletteKind {
    SPRITE_PALETTE_TEAM0,
    SPRITE_PALETTE_TEAM1,
    SPRITE_PALETTE_TEAM2,
    SPRITE_PALETTE_TEAM3,
    SPRITE_PALETTE_FLASH, // every visible pixel white
    SPRITE_PALETTE_COUNT
} SpritePaletteKind;

typedef struct SpriteInfo {
    i32 w, h;
    Vec2 pivot; // in sprite pixels: the point the sprite is placed and rotated around (grip, shoulder, feet...)
} SpriteInfo;

static inline SpriteId sprite_body(i32 variant, BodyPose pose) {
    return (SpriteId)(SPR_BODY_FIRST + variant * POSE_COUNT + (i32)pose);
}

SpriteInfo sprite_info(SpriteId id);
// Where the sprite is in an atlas (pixels).
Rect sprite_rect(SpriteId id);
// How far the head and torso of a pose sit below their usual place (breathing, stride).
i32 sprite_pose_bob(BodyPose pose);
// Atlas size in pixels.
void sprites_atlas_size(i32 *w, i32 *h);
// Fills `rgba` (atlas w * h * 4 bytes) with every sprite in the given team colors (r, g, b of the main and dark
// shades); `flash` gives the white silhouettes instead.
void sprites_build_atlas(u8 *rgba, const u8 team[3], const u8 team_dark[3], bool flash);
// Checks the sprite data (row lengths, palette characters). Returns false and describes the first problem.
bool sprites_validate(char *err, usize err_len);
