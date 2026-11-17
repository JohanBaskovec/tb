#include "core/rng.h"

#include "core/log.h"

void rng_seed(Rng *r, u64 seed, u64 stream) {
    r->state = 0u;
    r->inc = (stream << 1u) | 1u;
    rng_next_u32(r);
    r->state += seed;
    rng_next_u32(r);
}

u32 rng_next_u32(Rng *r) {
    u64 old = r->state;
    r->state = old * 6364136223846793005ULL + r->inc;
    u32 xorshifted = (u32)(((old >> 18u) ^ old) >> 27u);
    u32 rot = (u32)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
}

u32 rng_below(Rng *r, u32 bound) {
    ASSERT(bound > 0);
    // Rejection sampling removes modulo bias.
    u32 threshold = (0u - bound) % bound;
    for (;;) {
        u32 x = rng_next_u32(r);
        if (x >= threshold) {
            return x % bound;
        }
    }
}

i32 rng_range_i32(Rng *r, i32 lo, i32 hi) {
    ASSERT(lo <= hi);
    u32 span = (u32)hi - (u32)lo + 1u;
    return (i32)((u32)lo + rng_below(r, span));
}

f32 rng_f32(Rng *r) {
    return (f32)(rng_next_u32(r) >> 8) * (1.0f / 16777216.0f);
}
