// Scripted input sequences, for reproducible headless runs and tests.
//
// Syntax: steps separated by spaces or commas, each `KEYS:TICKS`.
//   KEYS: any of L R U D J F G S O P (left right up down jump fire grab switch order pause),
//         or a single '.' for "no keys".
//   Example: "R:60 RJ:12 .:30 U:90"  = run right 1s, run+jump 0.2s, idle 0.5s, up 1.5s.
#pragma once

#include "core/input_frame.h"

#define SCRIPT_MAX_STEPS 256

typedef struct Script {
    u32 masks[SCRIPT_MAX_STEPS];
    u32 ticks[SCRIPT_MAX_STEPS];
    u32 count;
    u32 step;  // current step
    u32 left;  // ticks left in the current step
} Script;

bool script_parse(Script *s, const char *text, char *err, usize err_len);
// Writes this tick's frame. Returns false (and releases all keys) once the script is over.
bool script_next(Script *s, InputFrame *frame);
bool script_finished(const Script *s);
u32 script_total_ticks(const Script *s);
