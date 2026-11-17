// PCG32 random number generator. Deterministic, state lives in the struct (no globals).
#pragma once

#include "core/types.h"

typedef struct Rng {
    u64 state;
    u64 inc;
} Rng;

void rng_seed(Rng *r, u64 seed, u64 stream);
u32 rng_next_u32(Rng *r);
// Uniform in [0, bound). bound must be > 0.
u32 rng_below(Rng *r, u32 bound);
// Uniform in [lo, hi], both inclusive.
i32 rng_range_i32(Rng *r, i32 lo, i32 hi);
// Uniform in [0, 1).
f32 rng_f32(Rng *r);
