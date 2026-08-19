#ifndef AW_MGBA_ADAPTER_H
#define AW_MGBA_ADAPTER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mCore;

struct mCore* aw_mgba_create(const char* rom_path, void* video_buffer, size_t stride);
void aw_mgba_set_video_buffer(struct mCore* core, void* buffer, size_t stride);
void aw_mgba_run_frame(struct mCore* core, uint16_t keys);

// Reads up to `max_samples` stereo sample frames from the core's audio buffer
// into `samples` (interleaved stereo int16). Returns the number of sample
// frames actually read.
size_t aw_mgba_read_audio(struct mCore* core, int16_t* samples, size_t max_samples);

// Returns the core's audio sample rate in Hz (0 on failure).
unsigned aw_mgba_audio_sample_rate(const struct mCore* core);

void aw_mgba_destroy(struct mCore* core);

#ifdef __cplusplus
}
#endif

#endif