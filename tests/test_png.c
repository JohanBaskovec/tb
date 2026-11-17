#include "test.h"

#include "core/png.h"

#include <stdlib.h>
#include <string.h>

static u32 be32(const u8 *p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static void test_checksums(void) {
    // Well-known check values.
    CHECK_EQ_U64(png_crc32(0, (const u8 *)"123456789", 9), 0xCBF43926u);
    CHECK_EQ_U64(png_adler32((const u8 *)"Wikipedia", 9), 0x11E60398u);
}

static void test_roundtrip(void) {
    // Large enough that the zlib stream needs several stored blocks (> 65535 bytes).
    const u32 w = 100, h = 200;
    const usize stride = (usize)w * 4 + 8; // padded stride, to test the stride path
    u8 *pix = (u8 *)calloc(stride * h, 1);
    CHECK(pix != NULL);
    for (u32 y = 0; y < h; y++) {
        for (u32 x = 0; x < w; x++) {
            u8 *p = pix + y * stride + x * 4;
            p[0] = (u8)(x * 2);
            p[1] = (u8)(y);
            p[2] = (u8)(x ^ y);
            p[3] = 255;
        }
    }
    const char *path = "test_png_roundtrip.png";
    CHECK(png_write_rgba(path, pix, w, h, stride));

    FILE *f = fopen(path, "rb");
    CHECK(f != NULL);
    if (!f) {
        free(pix);
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *file = (u8 *)malloc((usize)size);
    CHECK(fread(file, 1, (usize)size, f) == (usize)size);
    fclose(f);
    remove(path);

    static const u8 sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    CHECK(memcmp(file, sig, 8) == 0);

    // Walk the chunks, verify every CRC, collect IDAT.
    u8 *idat = (u8 *)malloc((usize)size);
    usize idat_len = 0;
    bool saw_ihdr = false, saw_iend = false;
    usize off = 8;
    while (off + 12 <= (usize)size) {
        u32 len = be32(file + off);
        const u8 *type = file + off + 4;
        const u8 *data = file + off + 8;
        CHECK(off + 12 + len <= (usize)size);
        u32 crc = png_crc32(0, type, 4 + len);
        CHECK_EQ_U64(crc, be32(data + len));
        if (memcmp(type, "IHDR", 4) == 0) {
            saw_ihdr = true;
            CHECK_EQ_U64(be32(data), w);
            CHECK_EQ_U64(be32(data + 4), h);
            CHECK(data[8] == 8 && data[9] == 6);
        } else if (memcmp(type, "IDAT", 4) == 0) {
            memcpy(idat + idat_len, data, len);
            idat_len += len;
        } else if (memcmp(type, "IEND", 4) == 0) {
            saw_iend = true;
        }
        off += 12 + len;
    }
    CHECK(saw_ihdr && saw_iend);
    CHECK_EQ_U64(off, size);

    // Decode the stored-block zlib stream and compare with the source pixels.
    const usize row_bytes = (usize)w * 4 + 1;
    const usize raw_len = row_bytes * h;
    u8 *raw = (u8 *)malloc(raw_len);
    CHECK(idat_len > 6 && idat[0] == 0x78);
    usize ip = 2, rp = 0;
    bool final_block = false;
    while (!final_block && ip + 5 <= idat_len) {
        final_block = (idat[ip] & 1) != 0;
        CHECK((idat[ip] & 6) == 0); // BTYPE = stored
        u32 n = (u32)idat[ip + 1] | ((u32)idat[ip + 2] << 8);
        u32 nn = (u32)idat[ip + 3] | ((u32)idat[ip + 4] << 8);
        CHECK_EQ_U64(n ^ 0xFFFFu, nn);
        ip += 5;
        CHECK(rp + n <= raw_len);
        memcpy(raw + rp, idat + ip, n);
        ip += n;
        rp += n;
    }
    CHECK(final_block);
    CHECK_EQ_U64(rp, raw_len);
    CHECK_EQ_U64(be32(idat + ip), png_adler32(raw, raw_len));
    for (u32 y = 0; y < h; y++) {
        CHECK(raw[y * row_bytes] == 0);
        CHECK(memcmp(raw + y * row_bytes + 1, pix + y * stride, (usize)w * 4) == 0);
    }

    free(raw);
    free(idat);
    free(file);
    free(pix);
}

int main(void) {
    RUN_TEST(test_checksums);
    RUN_TEST(test_roundtrip);
    TEST_MAIN_END();
}
