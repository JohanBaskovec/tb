#include "core/wav.h"

#include <stdio.h>

static void put_u32(u8 *p, u32 v) {
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}

static void put_u16(u8 *p, u32 v) {
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
}

bool wav_write_i16(const char *path, const i16 *samples, u32 frames, u32 channels, u32 rate) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    const u32 data_size = frames * channels * 2u;
    u8 h[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0};
    put_u32(h + 4, 36u + data_size);
    put_u16(h + 22, channels);
    put_u32(h + 24, rate);
    put_u32(h + 28, rate * channels * 2u);
    put_u16(h + 32, channels * 2u);
    put_u16(h + 34, 16);
    h[36] = 'd';
    h[37] = 'a';
    h[38] = 't';
    h[39] = 'a';
    put_u32(h + 40, data_size);
    bool ok = fwrite(h, 1, sizeof(h), f) == sizeof(h);
    for (u32 i = 0; ok && i < frames * channels; i++) { // little-endian whatever the machine
        u8 b[2];
        put_u16(b, (u32)(u16)samples[i]);
        ok = fwrite(b, 1, 2, f) == 2;
    }
    return fclose(f) == 0 && ok;
}
