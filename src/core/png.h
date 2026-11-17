// Minimal PNG writer (RGBA8, uncompressed deflate blocks). Used for headless screenshots.
#pragma once

#include "core/types.h"

// `stride` is the number of bytes between rows of `rgba`. Returns false on I/O error.
bool png_write_rgba(const char *path, const u8 *rgba, u32 width, u32 height, usize stride);

// Exposed for tests.
u32 png_crc32(u32 crc, const u8 *data, usize len);
u32 png_adler32(const u8 *data, usize len);
