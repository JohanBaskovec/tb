// Fixed engine-level constants shared by simulation, rendering and tests.
#pragma once

#include "core/types.h"

#define LOGICAL_W 640
#define LOGICAL_H 360
#define TILE_SIZE 16

#define TICK_HZ 60
#define TICK_DT (1.0f / (f32)TICK_HZ)

#define MAX_LOCAL_PLAYERS 4
#define MAX_TEAMS 4

#define MAX_LEVEL_W 256
#define MAX_LEVEL_H 128
#define MAX_DROP_ZONES 16

#define MAX_ENTITIES 256
