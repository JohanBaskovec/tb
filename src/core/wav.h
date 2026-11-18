// Minimal WAV writer (16-bit PCM). Used by `--export-audio` to listen to the synthesized sounds and music.
#pragma once

#include "core/types.h"

// `samples` holds `frames` frames of `channels` interleaved samples. Returns false on I/O error.
bool wav_write_i16(const char *path, const i16 *samples, u32 frames, u32 channels, u32 rate);
