#include "core/png.h"

#include "core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static u32 g_crc_table[256];
static bool g_crc_ready = false;

static void crc_init(void) {
    for (u32 n = 0; n < 256; n++) {
        u32 c = n;
        for (int k = 0; k < 8; k++) {
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        g_crc_table[n] = c;
    }
    g_crc_ready = true;
}

u32 png_crc32(u32 crc, const u8 *data, usize len) {
    if (!g_crc_ready) {
        crc_init();
    }
    u32 c = crc ^ 0xFFFFFFFFu;
    for (usize i = 0; i < len; i++) {
        c = g_crc_table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

u32 png_adler32(const u8 *data, usize len) {
    u32 a = 1, b = 0;
    for (usize i = 0; i < len; i++) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

static void put_be32(u8 *p, u32 v) {
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

static bool write_chunk(FILE *f, const char type[4], const u8 *data, u32 len) {
    u8 hdr[8];
    put_be32(hdr, len);
    memcpy(hdr + 4, type, 4);
    u32 crc = png_crc32(0, (const u8 *)type, 4);
    crc = png_crc32(crc, data, len);
    u8 tail[4];
    put_be32(tail, crc);
    return fwrite(hdr, 1, 8, f) == 8 && (len == 0 || fwrite(data, 1, len, f) == len) &&
           fwrite(tail, 1, 4, f) == 4;
}

bool png_write_rgba(const char *path, const u8 *rgba, u32 width, u32 height, usize stride) {
    ASSERT(width > 0 && height > 0 && stride >= (usize)width * 4);

    // Raw scanlines: one filter byte (0 = none) followed by the pixels.
    usize row_bytes = (usize)width * 4 + 1;
    usize raw_len = row_bytes * height;
    u8 *raw = (u8 *)malloc(raw_len);
    if (!raw) {
        return false;
    }
    for (u32 y = 0; y < height; y++) {
        raw[y * row_bytes] = 0;
        memcpy(raw + y * row_bytes + 1, rgba + (usize)y * stride, (usize)width * 4);
    }

    // zlib stream made of stored (uncompressed) deflate blocks.
    usize blocks = (raw_len + 65534) / 65535;
    usize z_len = 2 + raw_len + blocks * 5 + 4;
    u8 *z = (u8 *)malloc(z_len);
    if (!z) {
        free(raw);
        return false;
    }
    usize zp = 0;
    z[zp++] = 0x78;
    z[zp++] = 0x01;
    usize remaining = raw_len, src = 0;
    while (remaining > 0) {
        u16 n = (u16)(remaining > 65535 ? 65535 : remaining);
        z[zp++] = (remaining == n) ? 1 : 0;
        z[zp++] = (u8)(n & 0xFF);
        z[zp++] = (u8)(n >> 8);
        z[zp++] = (u8)(~n & 0xFF);
        z[zp++] = (u8)((~n >> 8) & 0xFF);
        memcpy(z + zp, raw + src, n);
        zp += n;
        src += n;
        remaining -= n;
    }
    put_be32(z + zp, png_adler32(raw, raw_len));
    zp += 4;
    free(raw);

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(z);
        LOG_ERROR("cannot open '%s' for writing", path);
        return false;
    }
    static const u8 sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    u8 ihdr[13];
    put_be32(ihdr, width);
    put_be32(ihdr + 4, height);
    ihdr[8] = 8;  // bit depth
    ihdr[9] = 6;  // color type: RGBA
    ihdr[10] = 0; // compression
    ihdr[11] = 0; // filter
    ihdr[12] = 0; // interlace
    bool ok = fwrite(sig, 1, 8, f) == 8 && write_chunk(f, "IHDR", ihdr, 13) &&
              write_chunk(f, "IDAT", z, (u32)zp) && write_chunk(f, "IEND", NULL, 0);
    free(z);
    if (fclose(f) != 0) {
        ok = false;
    }
    return ok;
}
